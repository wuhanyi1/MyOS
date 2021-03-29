#include "k_printf.h"
#include "init.h"
#include "thread.h"
#include "interrupt.h"
#include "console.h"
#include "process.h"

void k_thread_a(void*);
void k_thread_b(void*);
void u_prog_a(void);
void u_prog_b(void);
int test_var_a = 0, test_var_b = 0;

int main(void) {
   k_printf("I am kernel\n");
   init_all();

   thread_create((char*)"k_thread_a", 31, k_thread_a, (void*)"argA ");
   thread_create((char*)"k_thread_b", 31, k_thread_b, (void*)"argB ");
   process_execute((void*)u_prog_a, (char*)"user_prog_a");
   process_execute((void*)u_prog_b, (char*)"user_prog_b");

   intr_enable();
   while(1);
   return 0;
}

/* 在线程中运行的函数 */
void k_thread_a(void* arg) {     
   char* para = (char*)arg;
   while(1) {
      //console_put_str(" v_a:0x");
      console_put_int(test_var_a);
   }
}

/* 在线程中运行的函数 */
void k_thread_b(void* arg) {     
   char* para = (char*)arg;
   while(1) {
      //console_put_str(" v_b:0x");
      console_put_int(test_var_b);
   }
}

/* 测试用户进程 */
void u_prog_a(void) {
   while(1) {
      test_var_a++;
   }
}

/* 测试用户进程 */
void u_prog_b(void) {
   while(1) {
      test_var_b++;
   }
}
