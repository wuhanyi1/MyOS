#include "k_printf.h"
#include "init.h"
#include "thread.h"
#include "interrupt.h"
#include "console.h"
#include "process.h"
#include "syscall_init.h"
#include "syscall.h"
#include "stdio.h"
#include "memory.h"
#include "fs.h"
void printf(const char*,...);
void k_thread_a(void*);
void k_thread_b(void*);
void u_prog_a(void);
void u_prog_b(void);

int main(void) {
   k_printf("I am kernel\n");
   init_all();
   intr_enable();
   
   //process_execute((void*)u_prog_a, (char*)"u_prog_a");
   //process_execute((void*)u_prog_b, (char*)"u_prog_b");
   //thread_create((char*)"k_thread_a", 300, k_thread_a, (void*)"I am thread_a");
   //thread_create((char*)"k_thread_b", 31, k_thread_b, (void*)"I am thread_b");
   uint32_t fd = sys_open("/file1", O_CREAT);
   printf("fd:%d\n", fd);
   sys_write(fd, "hello,world\n", 12);
   sys_close(fd);
   printf("%d closed now\n", fd);
   while(1);
   return 0;
}

/* 在线程中运行的函数 */
void k_thread_a(void* arg) {     
   void* addr1 = sys_malloc(256);
   void* addr2 = sys_malloc(255);
   void* addr3 = sys_malloc(254);
   printfk(" k_thread_a malloc addr:%p,%p,%p\n", (int)addr1, (int)addr2, (int)addr3);
   printfk("phy add is:%p,%p,%p\n",addr_v2p((uint32_t)addr1),addr_v2p((uint32_t)addr2),addr_v2p((uint32_t)addr3));

   int cpu_delay = 100000;
   while(cpu_delay-- > 0);
   sys_free(addr1);
   sys_free(addr2);
   sys_free(addr3);
   while(1);
}

/* 在线程中运行的函数 */
void k_thread_b(void* arg) {     
   void* addr1 = sys_malloc(256);
   void* addr2 = sys_malloc(255);
   void* addr3 = sys_malloc(254);
   printfk(" k_thread_b malloc addr:%p,%p,%p\n", (int)addr1, (int)addr2, (int)addr3);
   printfk("phy add is:%p,%p,%p\n",addr_v2p((uint32_t)addr1),addr_v2p((uint32_t)addr2),addr_v2p((uint32_t)addr3));

   int cpu_delay = 100000;
   while(cpu_delay-- > 0);
   sys_free(addr1);
   sys_free(addr2);
   sys_free(addr3);
   while(1);
}

/* 测试用户进程 */
void u_prog_a(void) {
   task_struct* cur = running_thread();
   void* addr1 = (void*)malloc(256);
   void* addr2 = (void*)malloc(255);
   void* addr3 = (void*)malloc(254);
   printf(" prog_a malloc addr:%p,%p,%p\n", (int)addr1, (int)addr2, (int)addr3);
   printf("phy add is:%p,%p,%p\n",addr_v2p((uint32_t)addr1),addr_v2p((uint32_t)addr2),addr_v2p((uint32_t)addr3));
   printf("list size is %d\n",cur->u_block_desc[4].free_list.size);
   int cpu_delay = 100000;
   while(cpu_delay-- > 0);
   free(addr1);
   free(addr2);
   free(addr3);
   while(1);
}

/* 测试用户进程 */
void u_prog_b(void) {
   task_struct* cur = running_thread();
   void* addr1 = (void*)malloc(256);
   void* addr2 = (void*)malloc(255);
   void* addr3 = (void*)malloc(254);
   printf(" prog_b malloc addr:%p,%p,%p\n", (int)addr1, (int)addr2, (int)addr3);
   printf("phy add is:%p,%p,%p\n",addr_v2p((uint32_t)addr1),addr_v2p((uint32_t)addr2),addr_v2p((uint32_t)addr3));
   printf("list size is %d\n",cur->u_block_desc[4].free_list.size);
   int cpu_delay = 100000;
   while(cpu_delay-- > 0);
   free(addr1);
   free(addr2);
   free(addr3);
   while(1);
}
