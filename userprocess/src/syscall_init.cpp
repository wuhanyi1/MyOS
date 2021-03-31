#include "syscall_init.h"
#include "syscall.h"
#include "stdint.h"
#include "k_printf.h"
#include "thread.h"
#include "console.h"
#include "string.h"
#include "memory.h"

#define syscall_nr 32 
//using syscall = void*;
void* syscall_table[syscall_nr];//系统调用子功能函数入口地址

/* 返回当前任务的pid */
uint32_t sys_getpid(void) {
   return running_thread()->pid;
}

/* 打印字符串str(未实现文件系统前的版本) */
uint32_t sys_write(char* str) {
   console_put_str(str);
   return strlen(str);
}

/* 初始化系统调用 */
void syscall_init(void) {
   k_printf("syscall_init start\n");
   syscall_table[SYS_GETPID] = (void*)sys_getpid;
   syscall_table[SYS_WRITE] = (void*)sys_write;
   syscall_table[SYS_MALLOC] = (void*)sys_malloc;
   syscall_table[SYS_FREE] = (void*)sys_free;
   k_printf("syscall_init done\n");
}
