#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H
#include "stdint.h"
#include "bitmap.h"
//#include "thread.h"
#include "sync.h"
#include "list.h"

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



/* 内存池结构,生成3个实例用于管理内核内存池、用户内存池、内核虚拟地址池 */
struct pool {
   bitmap pool_bitmap;	 // 本内存池用到的位图结构,用于管理物理内存
   uint32_t addr_start = 0;	 // 本内存池所管理物理内存的起始地址
   uint32_t pool_size = 0;		 // 本内存池字节容量
   struct lock lock;
   
   void* addr_get(uint32_t pg_cnt);//在对应的地址池中申请page_cnt页，返回这些页的首地址
   void  addr_free(uint32_t addr);//释放对应地址池的从addr开始的pg_cnt页
};

#define DESC_CNT 7  //arena规格的数目

/* arena中内存块里面的链表节点，以后串到对应规格内存块描述符的内存块链表上 */
struct mem_block {
   list_elem free_elem;
};

/* 内存块描述符 */
struct mem_block_desc {
   uint32_t block_size;		 // 内存块大小
   uint32_t blocks_per_arena;	 // 本规格的arena可容纳此mem_block的数量.
   list free_list;	 // 目前可用的mem_block链表，由对应规格的arena集群中的内存块链接而成
};

/* 内存仓库arena元信息 */
struct arena {
   struct mem_block_desc* desc;	 // 此arena关联的mem_block_desc
/* large为ture时,cnt表示的是页框数。即分配超过1024B的内存块，就直接以页框进行分配
 * 否则cnt表示空闲mem_block数量 */
   uint32_t cnt;
   bool large;		  
   //mem_block* arena2block(uint32_t idx);
};

void* sys_malloc(uint32_t size);
void sys_free(void* ptr);
uint32_t addr_v2p(uint32_t vaddr);
struct task_struct* running_thread(void);
void* malloc_page(enum pool_flags pf,uint32_t pg_cnt);
uint32_t* pte_ptr(uint32_t vaddr);
uint32_t* pde_ptr(uint32_t vaddr);
void mem_init(void);
void* get_designed_vaddr(enum pool_flags pf,uint32_t vaddr);
void block_desc_init(struct mem_block_desc* desc_array);
//extern struct pool kernel_pool, user_pool;

#endif
