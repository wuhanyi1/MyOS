#include "memory.h"
#include "bitmap.h"
#include "stdint.h"
#include "global.h"
#include "k_printf.h"
#include "debug.h"
#include "string.h"
#include "thread.h"
#include "interrupt.h"

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

mem_block_desc k_block_descs[DESC_CNT];	// 内核内存块描述符数组，主要负责内核小内存块的分配

lock k_block_desc_lock; //内核的内存块描述符数组的锁，内核的这个是进程线程共享的

/*凡是涉及到对地址池操作的统统加锁*/

//在对应的内存池中申请pg_cnt页，并设置好位图，返回申请的页的地址.这个函数本身不是互斥的
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

/* 在pf的物理内存池中分配pg_cnt个页空间,并建立映射，并将物理页清0，返回虚拟地址 */
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
         //同样内核物理地址池也是公共资源，获得内核物理地址池的锁
         kernel_pool.lock.lock_acquire();
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
      cur->userprog_vaddr.lock.lock_acquire();//为了支持多现场，这里就上锁了
      vaddr_start = cur->userprog_vaddr.addr_get(pg_cnt);//在用户虚拟地址池获取pg_cnt页虚拟地址
      cur->userprog_vaddr.lock.lock_release();
      if(!vaddr_start) return nullptr;

      uint32_t vaddr = (uint32_t)vaddr_start, cnt = pg_cnt;

      /* 因为虚拟地址是连续的,但物理地址可以是不连续的,所以可以将虚拟地址和物理地址逐个做映射*/
      while (cnt-- > 0) {
         //用户物理地址池也是公共资源，需要上锁
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
   memset(vaddr_start,0,PG_SIZE * pg_cnt);//将申请到的物理页全部清0
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

         if(!ret) return nullptr;//一页分配失败，直接返回
         //2.检查这内核虚拟地址池这一页是否被分配出去了
         kernel_vaddr.lock.lock_acquire();
         uint32_t idx = (vaddr - kernel_vaddr.addr_start) / PG_SIZE;
         if(kernel_vaddr.pool_bitmap.bitmap_test(idx)){
            kernel_vaddr.lock.lock_release();
            k_printf("this addr has been use\n");
            return nullptr;
         }
         kernel_vaddr.lock.lock_release();

         //3.建立映射
         page_table_add((void*)vaddr,ret);//只要互斥的获得虚拟地址，虚拟地址定不同，访问的页表项自然也不同，不用互斥
      }else{//则是用户进程
         task_struct* cur = running_thread();
         //1.在用户物理内存池分配一片物理页
         user_pool.lock.lock_acquire();
         ret = user_pool.addr_get(1);
         user_pool.lock.lock_release();
         if(!ret) return nullptr;

         //2.检查这用户虚拟地址池这一页是否被分配出去了，为了扩展性(用户进程的多线程)，这里还是用个锁吧
         cur->userprog_vaddr.lock.lock_acquire();
         uint32_t idx = (vaddr - cur->userprog_vaddr.addr_start) / PG_SIZE;
         if(cur->userprog_vaddr.pool_bitmap.bitmap_test(idx)){
            cur->userprog_vaddr.lock.lock_release();
            k_printf("this addr has been use\n");
            return nullptr;
         }
         cur->userprog_vaddr.lock.lock_release();

         //3.建立映射
         page_table_add((void*)vaddr,ret);
      }

      memset((void*)vaddr,0,PG_SIZE);//将申请到的物理页全部清0
      return (void*)vaddr;//映射成功
}



/* 得到虚拟地址映射到的物理地址 */
uint32_t addr_v2p(uint32_t vaddr) {
   uint32_t* pte = pte_ptr(vaddr);
/* (*pte)的值是页表所在的物理页框地址,
 * 去掉其低12位的页表项属性+虚拟地址vaddr的低12位 */
   return ((*pte & 0xfffff000) + (vaddr & 0x00000fff));
}

/* 返回arena中第idx个内存块的地址 */
static mem_block* arena2block(arena* a, uint32_t idx) {
  return (mem_block*)((uint32_t)a + sizeof(arena) + idx * a->desc->block_size);
}

/* 返回内存块b所在的arena地址 */
static arena* block2arena(mem_block* b) {
   return (arena*)((uint32_t)b & 0xfffff000);
}

/* 在堆中申请size字节内存,并返回起始虚拟地址，如果是连续多页，虚拟地址是连续的 */
void* sys_malloc(uint32_t size) {
   enum pool_flags PF;
   //pool* mem_pool;
   uint32_t pool_size;
   mem_block_desc* descs;//指向mem_block_desc数组的起始地址
   task_struct* cur_thread = running_thread();

/* 判断用哪个内存池*/
   if (cur_thread->pgdir == nullptr) {     // 若为内核线程
      PF = PF_KERNEL; 
      pool_size = kernel_pool.pool_size;
      //k_printf("kernel memory pool\n");
      //mem_pool = &kernel_pool;
      descs = k_block_descs;
   } else {				      // 用户进程pcb中的pgdir会在为其分配页表时创建
      PF = PF_USER;
      pool_size = user_pool.pool_size;
      //mem_pool = &user_pool;
      descs = cur_thread->u_block_desc;
   }

   /* 若申请的内存不在内存池容量范围内则直接返回NULL */
   if (!(size > 0 && size < pool_size)) {
      return nullptr;
   }
   arena* a;
   mem_block* b;	
   //mem_pool->lock.lock_acquire();不用锁，我的分配物理页的函数就是保证互斥的
   //lock_acquire(&mem_pool->lock);

/* 超过最大内存块1024, 就分配页框 */
   if (size > 1024) {
      //k_printf("call malloc page!!!\n");
      uint32_t page_cnt = DIV_ROUND_UP(size + sizeof(struct arena), PG_SIZE);    // 向上取整需要的页框数
      //k_printf("page cnt is %d\n",page_cnt);
      a = (arena*)malloc_page(PF, page_cnt);//分配页框并建立映射

      if(a){//如果分配成功
         /* 对于分配的大块页框,将desc置为NULL, cnt置为页框数,large置为true */
         a->desc = nullptr;
         a->cnt = page_cnt;
         a->large = true;
         return (void*)(a + 1);		 // 跨过arena大小，把剩下的内存返回
      }
      else return nullptr;

   } else {    // 若申请的内存小于等于1024,可在各种规格的mem_block_desc中去适配
      uint8_t desc_idx;
      /* 从内存块描述符中匹配合适的内存块规格 */
      for (desc_idx = 0; desc_idx < DESC_CNT; desc_idx++) {
	      if (size <= descs[desc_idx].block_size)   // 从小往大后,找到后退出
	         break;
      }
      //k_printf("desc_idx is %p\n",desc_idx);
   /* 若mem_block_desc的free_list中已经没有可用的mem_block,就创建新的arena提供mem_block */
      if (descs[desc_idx].free_list.is_empty()) {
	      a = (arena*)malloc_page(PF, 1);       // 分配1页框做为arena
         if (!a) return nullptr;//如果分配失败，直接返回

      /* 对于分配的小块内存,将desc置为相应内存块描述符, cnt置为此arena可用的内存块数,large置为false */
	      a->desc = &descs[desc_idx];
	      a->large = false;
	      a->cnt = descs[desc_idx].blocks_per_arena;

	      enum intr_status old_status = intr_disable();

	      /* 开始将arena拆分成内存块,并添加到内存块描述符的free_list中 */
	      for (uint32_t block_idx = 0; block_idx < descs[desc_idx].blocks_per_arena; block_idx++) {
	         b = arena2block(a, block_idx);
	         //ASSERT(!elem_find(&a->desc->free_list, &b->free_elem));
            a->desc->free_list.push_front(&b->free_elem);
            //list_append(&a->desc->free_list, &b->free_elem);	
	      }
	      intr_set_status(old_status);
      }    
      
      /* 开始分配内存块 */
         
         b = elem2entry(mem_block, free_elem,descs[desc_idx].free_list.pop_back());
         memset(b, 0, descs[desc_idx].block_size);//将分配的小内存块全部清0

         a = block2arena(b);  // 获取内存块b所在的arena
         enum intr_status old_status = intr_disable();
         a->cnt--;		   // 将此arena中的空闲内存块数减1,这里是公共资源，原子
         intr_set_status(old_status);
         return (void*)b;
      }
}
/* 释放对应地址池的从addr开始的1页，函数保证了对公共结构的原子性修改 */
void pool::addr_free(uint32_t addr){
   uint32_t bit_idx = (addr - addr_start) / PG_SIZE;//获得位图下标
   enum intr_status old_status = intr_disable();
   pool_bitmap.bitmap_set(bit_idx,0);
   intr_set_status(old_status);
}

// /* 将物理地址pg_phy_addr回收到物理内存池,这里要互斥 */
// void pfree(uint32_t pg_phy_addr) {
//    struct pool* mem_pool;
//    uint32_t bit_idx = 0;
//    if (pg_phy_addr >= user_pool.addr_start) {     // 用户物理内存池
//       mem_pool = &user_pool;
//       bit_idx = (pg_phy_addr - user_pool.addr_start) / PG_SIZE;
//    } else {	  // 内核物理内存池
//       mem_pool = &kernel_pool;
//       bit_idx = (pg_phy_addr - kernel_pool.addr_start) / PG_SIZE;
//    }
//    //对物理内存池操作，要互斥上锁
//    mem_pool->lock.lock_acquire();
//    mem_pool->pool_bitmap.bitmap_set(bit_idx,0);
//    mem_pool->lock.lock_acquire();
//    //bitmap_set(&mem_pool->pool_bitmap, bit_idx, 0);	 // 将位图中该位清0
// }

/* 去掉页表中虚拟地址vaddr的映射,只去掉vaddr对应的pte。一次解除一个pte就是解除一个虚拟页
所以跟物理内存是否再被分配出去无关 */
static void page_table_pte_remove(uint32_t vaddr) {
   uint32_t* pte = pte_ptr(vaddr);
   *pte &= ~PG_P_1;	// 将页表项pte的P位置0
   asm volatile ("invlpg %0"::"m" (vaddr):"memory");    //更新tlb，将虚拟地址vaddr的变化告诉更新到TLB
}

// /* 在虚拟地址池中释放以_vaddr起始的连续pg_cnt个虚拟页地址 */
// static void vaddr_remove(enum pool_flags pf, void* _vaddr, uint32_t pg_cnt) {
//    uint32_t bit_idx_start = 0, vaddr = (uint32_t)_vaddr, cnt = 0;

//    if (pf == PF_KERNEL) {  // 内核虚拟内存池
//       bit_idx_start = (vaddr - kernel_vaddr.addr_start) / PG_SIZE;
//       while(cnt < pg_cnt) {
//          //内核虚拟地址池是功能资源，需要上锁
//          kernel_vaddr.lock.lock_acquire();
//          kernel_vaddr.pool_bitmap.bitmap_set(bit_idx_start + cnt++, 0);
//          kernel_vaddr.lock.lock_release();
// 	      //bitmap_set(&kernel_vaddr.vaddr_bitmap, );
//       }
//    } else {  // 用户虚拟内存池
//       struct task_struct* cur_thread = running_thread();
//       bit_idx_start = (vaddr - cur_thread->userprog_vaddr.addr_start) / PG_SIZE;
//       while(cnt < pg_cnt) {
//          cur_thread->userprog_vaddr.lock.lock_acquire();
//          cur_thread->userprog_vaddr.pool_bitmap.bitmap_set(bit_idx_start + cnt++, 0);
//          cur_thread->userprog_vaddr.lock.lock_release();
// 	      //bitmap_set(&cur_thread->userprog_vaddr.vaddr_bitmap, );
//       }
//    }
// }

/* 释放以虚拟地址vaddr为起始的cnt个虚拟页以及对应物理页框,还要释放映射 */
void mfree_page(enum pool_flags pf, void* _vaddr, uint32_t pg_cnt) {
   uint32_t pg_phy_addr;
   uint32_t vaddr = (uint32_t)_vaddr, page_cnt = 0;

   //如果是物理内存池
   if(pf == PF_KERNEL){
      for(int i = 0;i < pg_cnt;i++){
         pg_phy_addr = addr_v2p(vaddr + i * PG_SIZE);  // 获取虚拟地址vaddr对应的物理地址
         //1.解除PTE映射
         page_table_pte_remove(vaddr + i * PG_SIZE);
         //2.释放物理地址池
         kernel_pool.addr_free(pg_phy_addr);
         //3.释放虚拟地址池
         kernel_vaddr.addr_free(vaddr + i * PG_SIZE);
      }
   }else{//如果是用户地址池
      task_struct* cur = running_thread();
      for(int i = 0;i < pg_cnt;i++){
         pg_phy_addr = addr_v2p(vaddr + i * PG_SIZE);  // 获取虚拟地址vaddr对应的物理地址
         //1.解除PTE映射
         page_table_pte_remove(vaddr + i * PG_SIZE);
         //2.释放物理地址池
         user_pool.addr_free(pg_phy_addr);
         //3.释放虚拟地址池
         cur->userprog_vaddr.addr_free(vaddr + i * PG_SIZE);
      }
   }
}

/* 回收内存ptr */
void sys_free(void* ptr) {
   if (ptr == nullptr) return;
   
   enum pool_flags PF;
   struct pool* mem_pool;

   /* 判断是线程还是进程 */
   if (running_thread()->pgdir == nullptr) {
      //k_printf("this is thread\n");
	   PF = PF_KERNEL; 
	   mem_pool = &kernel_pool;
   } else {
      //k_printf("this is process\n");
	   PF = PF_USER;
	   mem_pool = &user_pool;
   }

   mem_pool->lock.lock_acquire();   
   
   mem_block* b = (mem_block*)ptr;
   arena* a = block2arena(b);	     // 把mem_block转换成arena,获取元信息

   if (a->desc == nullptr && a->large == true) { // 大于1024的内存
	   mfree_page(PF, (void*)a, a->cnt); 
   } else {				 // 小于等于1024的内存块
	 /* 先将内存块回收到free_list */
    a->desc->free_list.push_front(&b->free_elem);
	 //list_append(&a->desc->free_list, &b->free_elem);

	 /* 再判断此arena中的内存块是否都是空闲,如果是就释放arena */
	 if (++(a->cnt) == a->desc->blocks_per_arena) {
         //k_printf("free the whole page\n");
         uint32_t block_idx;
         //将arena中的所有的节点从这个描述符的链表中删除
	      for (block_idx = 0; block_idx < a->desc->blocks_per_arena; block_idx++) {
	         struct mem_block*  b = arena2block(a, block_idx);
            a->desc->free_list.remove(&b->free_elem);
	      }
	      mfree_page(PF, a, 1);//释放这个arena所在页
	   } 
   }   
   mem_pool->lock.lock_release();
   //lock_release(&mem_pool->lock); 
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

/* 为malloc做准备 */
void block_desc_init(mem_block_desc* desc_array) {				   
   
   uint16_t block_size = 16;

   /* 初始化每个mem_block_desc描述符 */
   for (uint16_t desc_idx = 0; desc_idx < DESC_CNT; desc_idx++) {
      desc_array[desc_idx].block_size = block_size;

      /* 初始化arena中的内存块数量 */
      desc_array[desc_idx].blocks_per_arena = (PG_SIZE - sizeof(struct arena)) / block_size;	  
   
      desc_array[desc_idx].free_list.init();//初始化内存块空闲链表
      //list_init(&desc_array[desc_idx].free_list);

      block_size *= 2;         // 更新为下一个规格内存块
   }
}

/* 内存管理部分初始化入口 */
void mem_init() {
   k_printf("mem_init start\n");
   uint32_t mem_bytes_total = (*(uint32_t*)(0xb00));
   k_block_desc_lock.init();//初始化内核内存块描述符锁
   mem_pool_init(mem_bytes_total);	  // 初始化内存池
   block_desc_init(k_block_descs);     //初始化小物理块管理结构
   k_printf("mem_init done\n");
}
