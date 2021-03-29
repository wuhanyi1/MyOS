#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H
#include "stdint.h"
#include "bitmap.h"
//#include "thread.h"
#include "sync.h"

/* 内存池标记,用于判断用哪个内存池 */
enum pool_flags {
   PF_KERNEL = 1,    // 内核内存池
   PF_USER = 2	     // 用户内存池
};

#define	 PG_P_1	  1	// 页表项或页目录项存在属性位
#define	 PG_P_0	  0	// 页表项或页目录项存在属性位
#define	 PG_RW_R  0	// R/W 属性位值, 读/执行
#define	 PG_RW_W  2	// R/W 属性位值, 读/写/执行
#define	 PG_US_S  0	// U/S 属性位值, 系统级
#define	 PG_US_U  4	// U/S 属性位值, 用户级

// /* 用于虚拟地址管理 */
// struct virtual_addr {
// /* 虚拟地址用到的位图结构，用于记录哪些虚拟地址被占用了。以页为单位。*/
//    struct bitmap vaddr_bitmap;
// /* 管理的虚拟地址 */
//    uint32_t vaddr_start;
// };


/* 内存池结构,生成3个实例用于管理内核内存池、用户内存池、内核虚拟地址池 */
struct pool {
   bitmap pool_bitmap;	 // 本内存池用到的位图结构,用于管理物理内存
   uint32_t addr_start = 0;	 // 本内存池所管理物理内存的起始地址
   uint32_t pool_size = 0;		 // 本内存池字节容量
   struct lock lock;
   
   void* addr_get(uint32_t pg_cnt);//在对应的地址池中申请page_cnt页，返回这些页的首地址
   
};
uint32_t addr_v2p(uint32_t vaddr);
struct task_struct* running_thread(void);
void* malloc_page(enum pool_flags pf,uint32_t pg_cnt);
uint32_t* pte_ptr(uint32_t vaddr);
uint32_t* pde_ptr(uint32_t vaddr);
void mem_init(void);
void* get_designed_vaddr(enum pool_flags pf,uint32_t vaddr);
//extern struct pool kernel_pool, user_pool;

#endif
