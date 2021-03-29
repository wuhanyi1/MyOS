#include "debug.h"
#include "k_printf.h"
#include "interrupt.h"

/* 打印文件名,行号,函数名,条件并使程序悬停 */
void panic_spin(char* filename,	       \
	        int line,	       \
		const char* func,      \
		const char* condition) \
{
   intr_disable();	// 因为有时候会单独调用panic_spin,所以在此处关中断。
   k_printf("\n\n\n!!!!! error !!!!!\n");
   k_printf("filename:");k_printf(filename);k_printf("\n");
   k_printf("line:0x");put_int(line);k_printf("\n");
   k_printf("function:");k_printf((char*)func);k_printf("\n");
   k_printf("condition:");k_printf((char*)condition);k_printf("\n");
   while(1);
}
