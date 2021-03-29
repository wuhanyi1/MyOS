#include "memory.h"
#include "bitmap.h"
#include "stdint.h"
#include "global.h"
#include "k_printf.h"
#include "debug.h"
#include "string.h"
#include "thread.h"

#define PG_SIZE 4096

/***************  位图地址 ********************
 * 因为0xc009f000是内核主线程栈顶，0xc009e000是内核主线程的pcb.
 * 一个页框大小的位图可表示128M内存, 位图位置安排在地址0xc009a000,
 * 这样本系统最大支持4个页框的位图,即512M */
#define MEM_BITMAP_BASE 0xc009a000
/*************************************/

#define PDE_IDX(addr) ((addr & 0xffc00000) >> 22)
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12)

/* 0xc0000000是内核从虚拟地址3G起. 0x100000意指跨过低端1M内存,使虚拟地址在逻辑上连续 */
#define K_HEAP_START 0xc0100000

pool kernel_pool, user_pool, kernel_vaddr;      // 生成内核内存池、用户内存池、内核虚拟地址池

//在对应的内存池中申请pg_cnt页，并设置好位图，返回申请的页的地址
void* pool::addr_get(uint32_t pg_cnt) {
   int bit_idx_start = -1;
   uint32_t cnt = 0;
   bit_idx_start  = pool_bitmap.bitmap_alloc(pg_cnt);//在自己的位图中申请pg_cnt页
   
   if (bit_idx_start == -1) { //申请失败
	   return nullptr;
   }

   while(cnt < pg_cnt) { //将内核虚拟地址池的位图的相应位置1
	   pool_bitmap.bitmap_set(bit_idx_start + cnt++, 1);
   }

   return (void*)(addr_start + bit_idx_start * PG_SIZE);
}

/* 得到虚拟地址vaddr对应的pte指针*/
uint32_t* pte_ptr(uint32_t vaddr) {
   /* 先访问到页表自己 + \
    * 再用页目录项pde(页目录内页表的索引)做为pte的索引访问到页表 + \
    * 再用pte的索引做为页内偏移*/
   uint32_t* pte = (uint32_t*)(0xffc00000 + \
	 ((vaddr & 0xffc00000) >> 10) + \
	 PTE_IDX(vaddr) * 4);
   return pte;
}

/* 得到虚拟地址vaddr对应的pde的指针 */
uint32_t* pde_ptr(uint32_t vaddr) {
   /* 0xfffff是用来访问到页表本身所在的地址 */
   uint32_t* pde = (uint32_t*)((0xfffff000) + PDE_IDX(vaddr) * 4);
   return pde;
}

/* 页表中添加虚拟地址_vaddr与物理地址_page_phyaddr的映射 */
void page_table_add(void* _vaddr, void* _page_phyaddr) {
   uint32_t vaddr = (uint32_t)_vaddr, page_phyaddr = (uint32_t)_page_phyaddr;
   //获得这个虚拟地址的pde与pte地址
   uint32_t* pde = pde_ptr(vaddr);
   uint32_t* pte = pte_ptr(vaddr);

/************************   注意   *************************
 * 执行*pte,会访问到空的pde。所以确保pde创建完成后才能执行*pte,
 * 否则会引发page_fault。因此在*pde为0时,*pte只能出现在下面else语句块中的*pde后面。
 * *********************************************************/
   /* 先判断PDE的P位，若为1,则表示对应页表已存在 */
   if (*pde & 0x00000001) {	 // 页目录项和页表项的第0位为P,此处判断目录项是否存在
      //k_printf("PDE exist!!\n");
      // 因为是建立映射,pte就应该不存在,即PTE的p位为0，多判断一下放心
      if (!(*pte & 0x00000001)) {   
	      *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);    // US=1,RW=1,P=1
      }
      else {			    //应该不会执行到这，因为上面的ASSERT会先执行。
         k_printf("pte exist!!\n");
	      *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);      // US=1,RW=1,P=1
      }
   }
   else {			    // pde为0，说明页表不存在，则在内核内存池申请一个页面作为页表
      /* 页表中用到的页框一律从内核空间分配 */
      kernel_pool.lock.lock_acquire();//获得内核内存池锁
      uint32_t pde_phyaddr = (uint32_t)kernel_pool.addr_get(1);//注意页表本身不需要再建立映射
      kernel_pool.lock.lock_release();//用完就放

      *pde = (pde_phyaddr | PG_US_U | PG_RW_W | PG_P_1);

      /* 分配到的物理页地址pde_phyaddr对应的物理内存清0,
       * 避免里面的陈旧数据变成了页表项,从而让页表混乱.
       * 访问到pde对应的物理地址,用pte取高20位便可.
       * 因为pte是基于该pde对应的物理地址内再寻址,
       * 把低12位置0便是该pde对应的物理页的起始*/
      memset((void*)((int)pte & 0xfffff000), 0, PG_SIZE);
         
      //ASSERT(!(*pte & 0x00000001));
      *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);      // US=1,RW=1,P=1
   }
}

/* 在pf的物理内存池中分配pg_cnt个页空间,并建立映射，返回虚拟地址 */
void* malloc_page(enum pool_flags pf,uint32_t pg_cnt) {
   void* vaddr_start;
/***********   malloc_page的原理是三个动作的合成:   ***********
      1通过vaddr_get在虚拟内存池中申请虚拟地址
      2通过palloc在物理内存池中申请物理页
      3通过page_table_add将以上得到的虚拟地址和物理地址在页表中完成映射
***************************************************************/
   //在内核内存池分配pg_cnt个页面，并建立好映射，返回地址
   if(pf == PF_KERNEL){
      //会用到内核虚拟地址池，内核虚拟地址池的锁获得不到，直接阻塞
      kernel_vaddr.lock.lock_acquire();
      vaddr_start = kernel_vaddr.addr_get(pg_cnt);//在内核虚拟地址内存池申请pg_cnt页
      kernel_vaddr.lock.lock_release();//用完就放

      if (vaddr_start == nullptr) return nullptr;

      uint32_t vaddr = (uint32_t)vaddr_start, cnt = pg_cnt;

      /* 因为虚拟地址是连续的,但物理地址可以是不连续的,所以可以将虚拟地址和物理地址逐个做映射*/
      while (cnt-- > 0) {
         kernel_pool.lock.lock_acquire();//获得内核物理地址池的锁
         void* page_phyaddr = kernel_pool.addr_get(1);//在自己的物理内存池分配一页，用来映射
         kernel_pool.lock.lock_release();//用完直接释放
         
         if (page_phyaddr == nullptr) {  // 失败时要将曾经已申请的虚拟地址和物理页全部回滚，在将来完成内存回收时再补充
	         return nullptr;
         }
         page_table_add((void*)vaddr, page_phyaddr); // 在页表中做映射 
         vaddr += PG_SIZE;		 // 下一个虚拟页
      }
   }
   else{//如果是用户物理内存池,这一段代码是在内核里面执行的
      task_struct* cur = running_thread();
      vaddr_start = cur->userprog_vaddr.addr_get(pg_cnt);//在用户虚拟地址池获取pg_cnt页虚拟地址
      if(!vaddr_start) return nullptr;

      uint32_t vaddr = (uint32_t)vaddr_start, cnt = pg_cnt;

      /* 因为虚拟地址是连续的,但物理地址可以是不连续的,所以可以将虚拟地址和物理地址逐个做映射*/
      while (cnt-- > 0) {
         user_pool.lock.lock_acquire();//对用户物理内存池上锁
         void* page_phyaddr = user_pool.addr_get(1);//在自己的物理内存池分配一页，用来映射
         user_pool.lock.lock_release();//用完就放
         
         if (page_phyaddr == nullptr) {  // 失败时要将曾经已申请的虚拟地址和物理页全部回滚，在将来完成内存回收时再补充
	         return nullptr;
         }
         page_table_add((void*)vaddr, page_phyaddr); // 确保此时是用户的CR3
         vaddr += PG_SIZE;		 // 下一个虚拟页
      }
      
   /* (0xc0000000 - PG_SIZE)做为用户3级栈已经在start_process被分配 */
      //ASSERT((uint32_t)vaddr_start < (0xc0000000 - PG_SIZE));
   }
   
   return vaddr_start;
}

//在pf内存池分配物理地址，和指定虚拟地址建立映射，并返回虚拟地址.这个就是分配一页
void* get_designed_vaddr(enum pool_flags pf,uint32_t vaddr){
      void* ret = nullptr;
      if(pf == PF_KERNEL){
         //1.在内核地址池申请分配一片物理页
         kernel_pool.lock.lock_acquire();
         ret = kernel_pool.addr_get(1);
         kernel_pool.lock.lock_release();

         if(!ret) return nullptr;
         //2.建立映射
         page_table_add((void*)vaddr,ret);//只要互斥的获得虚拟地址，虚拟地址定不同，访问的页表项自然也不同，不用互斥
      }else{//则是用户进程
         //1.在用户物理内存池分配一片物理页
         user_pool.lock.lock_acquire();
         ret = user_pool.addr_get(1);
         user_pool.lock.lock_release();
         if(!ret) return nullptr;

         //2.建立映射
         page_table_add((void*)vaddr,ret);
      }
      return (void*)vaddr;//映射成功
}



/* 得到虚拟地址映射到的物理地址 */
uint32_t addr_v2p(uint32_t vaddr) {
   uint32_t* pte = pte_ptr(vaddr);
/* (*pte)的值是页表所在的物理页框地址,
 * 去掉其低12位的页表项属性+虚拟地址vaddr的低12位 */
   return ((*pte & 0xfffff000) + (vaddr & 0x00000fff));
}


/* 初始化内存池 */
static void mem_pool_init(uint32_t all_mem) {
   k_printf("   mem_pool_init start\n");
   uint32_t page_table_size = PG_SIZE * 256;	  // 页表大小= 1页的页目录表+第0和第768个页目录项指向同一个页表+
                                                  // 第769~1022个页目录项共指向254个页表,共256个页框
   uint32_t used_mem = page_table_size + 0x100000;	  // 0x100000为低端1M内存
   uint32_t free_mem = all_mem - used_mem;
   uint16_t all_free_pages = free_mem / PG_SIZE;		  // 1页为4k,不管总内存是不是4k的倍数,
								  // 对于以页为单位的内存分配策略，不足1页的内存不用考虑了。
   uint16_t kernel_free_pages = all_free_pages / 2;
   uint16_t user_free_pages = all_free_pages - kernel_free_pages;

/* 为简化位图操作，余数不处理，坏处是这样做会丢内存。
好处是不用做内存的越界检查,因为位图表示的内存少于实际物理内存*/
   uint32_t kbm_length = kernel_free_pages / 8;			  // Kernel BitMap的长度,位图中的一位表示一页,以字节为单位
   uint32_t ubm_length = user_free_pages / 8;			  // User BitMap的长度.

   uint32_t kp_start = used_mem;				  // Kernel Pool start,内核内存池的起始地址
   uint32_t up_start = kp_start + kernel_free_pages * PG_SIZE;	  // User Pool start,用户内存池的起始地址

   kernel_pool.addr_start = kp_start;
   user_pool.addr_start   = up_start;

   kernel_pool.pool_size = kernel_free_pages * PG_SIZE;
   user_pool.pool_size	 = user_free_pages * PG_SIZE;

   kernel_pool.pool_bitmap.btmp_bytes_len = kbm_length;
   user_pool.pool_bitmap.btmp_bytes_len	  = ubm_length;

/*********    内核内存池和用户内存池位图   ***********
 *   位图是全局的数据，长度不固定。
 *   全局或静态的数组需要在编译时知道其长度，
 *   而我们需要根据总内存大小算出需要多少字节。
 *   所以改为指定一块内存来生成位图.
 *   ************************************************/
// 内核使用的最高地址是0xc009f000,这是主线程的栈地址.(内核的大小预计为70K左右)
// 32M内存占用的位图是2k.内核内存池的位图先定在MEM_BITMAP_BASE(0xc009a000)处.
   kernel_pool.pool_bitmap.bits = (uint8_t*)MEM_BITMAP_BASE;
							       
/* 用户内存池的位图紧跟在内核内存池位图之后 */
   user_pool.pool_bitmap.bits = (uint8_t*)(MEM_BITMAP_BASE + kbm_length);
   /******************** 输出内存池信息 **********************/
   k_printf("      kernel_pool_bitmap_start: %p \n",(int)kernel_pool.pool_bitmap.bits);
   k_printf(" kernel_pool_phy_addr_start: %p\n",kernel_pool.addr_start);
   
   k_printf("      user_pool_bitmap_start: %p \n",(int)user_pool.pool_bitmap.bits);
   k_printf(" user_pool_phy_addr_start: %p \n",user_pool.addr_start);

   /* 将内核与用户物理内存池位图内容置0*/
   kernel_pool.pool_bitmap.init();
   user_pool.pool_bitmap.init();

   /* 下面初始化内核虚拟地址的位图,按实际物理内存大小生成数组。*/
   kernel_vaddr.pool_bitmap.btmp_bytes_len = kbm_length;      // 用于维护内核堆的虚拟地址,所以要和内核内存池大小一致

  /* 位图的数组指向一块未使用的内存,目前定位在内核内存池和用户内存池之外*/
   kernel_vaddr.pool_bitmap.bits = (uint8_t*)(MEM_BITMAP_BASE + kbm_length + ubm_length);

   kernel_vaddr.addr_start = K_HEAP_START;
   kernel_vaddr.pool_bitmap.init();

   //初始化两个物理内存池的锁,还有虚拟地址池的锁
   kernel_pool.lock.init();
   user_pool.lock.init();
   kernel_vaddr.lock.init();

   //kernel_vaddr.lock.init();//初始化虚拟地址的锁
   k_printf(" kernel_vaddr_pool_bitmap_start: %p \n",(int)kernel_vaddr.pool_bitmap.bits);
   k_printf(" kernel_pool_vaddr_start: %p \n",kernel_vaddr.addr_start);

   k_printf("   mem_pool_init done\n");
}

/* 内存管理部分初始化入口 */
void mem_init() {
   k_printf("mem_init start\n");
   uint32_t mem_bytes_total = (*(uint32_t*)(0xb00));
   mem_pool_init(mem_bytes_total);	  // 初始化内存池
   k_printf("mem_init done\n");
}
