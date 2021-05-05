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
   k_printf("filename:%s\n",filename);
   k_printf("line:%d\n",line);
   k_printf("function:%s\n",func);
   k_printf("condition:%s\n",(char*)condition);
   while(1);
}
