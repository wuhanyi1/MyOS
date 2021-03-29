#ifndef __THREAD_THREAD_H
#define __THREAD_THREAD_H
#include "stdint.h"
#include "list.h"
#include "memory.h"

/* 自定义通用函数类型,它将在很多线程函数中做为形参类型 */
using thread_func = void(void*);

/* 进程或线程的状态 */
enum task_status {
   TASK_RUNNING,
   TASK_READY,
   TASK_BLOCKED,
   TASK_WAITING,
   TASK_HANGING,
   TASK_DIED
};

/***********   中断栈intr_stack   ***********
 * 此结构的内容是中断发生后通用中断处理程序压栈线程或进程的上下文环境的实体内容:
 * 进程或线程被外部中断或软中断打断时,会按照此结构压入上下文
 * 寄存器,  intr_exit中的出栈操作是此结构的逆操作
 * 此栈在线程自己的内核栈中位置固定,所在页的最顶端
********************************************/
//这个结构从低地址到高地址依次向下排列，正好对应栈从高地址向下增长的特点。所以这个结构的第一个变量就是最后一个压入的
struct intr_stack {
    uint32_t vec_no;	 // kernel.S 宏VECTOR中push %1压入的中断号
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp_dummy;	 // 虽然pushad把esp也压入,但esp是不断变化的,所以会被popad忽略
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;

/* 以下由cpu从低特权级进入高特权级时压入 */
    uint32_t err_code;		 // err_code会被压入在eip之后
    void (*eip) (void);      // eip，他是个偏移量，在平坦模式就是虚拟地址，可以用函数指针来保存
    uint32_t cs;
    uint32_t eflags;
    void* esp;
    uint32_t ss;
};

/***********  线程栈thread_stack  ***********
 * 线程自己的栈,用于存储线程中待执行的函数
 * 此结构在线程自己的内核栈中位置不固定,
 * 用在switch_to时保存线程环境。
 * 实际位置取决于实际运行情况。
 ******************************************/
struct thread_stack {
/* 这四个是根据ABI约束，被调函数switch_to可能用到全局下面这四个全局寄存器，
那么就会破坏主调函数shedule的值，为了保证返回主调函数schedule时值这些寄存器还具有相同的值
所以就需要被调函数来保存 */
   uint32_t ebp;
   uint32_t ebx;
   uint32_t edi;
   uint32_t esi;

/* 线程第一次执行时,eip存放的是待调用的函数kernel_thread的地址
在第2,3...次执行时,eip是指向switch_to的返回地址*/
   void (*eip) (thread_func* func, void* func_arg);

/*以下仅供进程、线程第一次被调度上cpu时使用，由于第一次执行线程记录的函数是由kernel_thread这个函数
调用的，而kernel_thread是void (thread_func* func, void* func_arg);这种函数类型，所以这个函数需要
两个参数，根据ABI约束，参数自右向左压栈
*/

/* 参数unused_ret只为占位置充数为返回地址，因为我们kernel_thread是用c++写的，所以他
编译时默认上面有一个主调函数的栈帧，那么主调函数的栈帧的最后一个内容就是主调函数的返回地址
但是实际上kernel_thread这个函数没有主调函数，但是还是要假装有，所以需要一个占位的 */
   void (*unused_retaddr);
   thread_func* function;   // 由Kernel_thread所调用的函数名
   void* func_arg;    // 由Kernel_thread所调用的函数所需的参数
};


/* 进程或线程的pcb,程序控制块 */
struct task_struct {
   uint32_t* self_kstack;	 // 记录线程内核栈顶指针此时的位置
   enum task_status status;
   char name[16];
   uint8_t priority;    // 我们这里优先级就是每个进程、线程能运行的时钟周期数
   uint8_t ticks;	   // 每次在处理器上执行的时间嘀嗒数

/* 此任务自上cpu运行后至今占用了多少cpu嘀嗒数,
 * 也就是此任务执行了多久*/
   uint32_t elapsed_ticks;

/* general_tag的作用是用于线程在一般的队列中的结点 */
   list_elem general_tag;				    

/* all_list_tag的作用是用于线程队列thread_all_list中的结点 */
   list_elem all_list_tag;

   uint32_t* pgdir;              // 进程自己页目录表的虚拟地址，这个变量不为NULL就是进程，为NULL就是线程
   pool userprog_vaddr;   // 用户进程的虚拟地址池,内核线程的PCB这个字段没用
   uint32_t stack_magic;	 // 用这串数字做栈的边界标记,用于检测栈的溢出，即防止栈向下扩展太多覆盖掉PCB的内容
   void thread_block(enum task_status stat);
   void thread_unblock();
   void init(char* name,int prio,thread_func function, void* func_arg);
};

task_struct* thread_create(char* name, int prio, thread_func function, void* func_arg);
struct task_struct* running_thread(void);
void schedule(void);
void thread_init(void);
#endif
