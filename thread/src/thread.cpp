#include "thread.h"
#include "stdint.h"
#include "string.h"
#include "global.h"
//#include "debug.h"
#include "interrupt.h"
#include "k_printf.h"
#include "process.h"
#include "memory.h"
#include "sync.h"
#define PG_SIZE 4096

task_struct* idle_thread;    // idle线程
task_struct* main_thread;    // 主线程PCB,也就是OS内核一直在执行的执行流，就是OS的main函数开启的执行流
list thread_ready_list;	    // 就绪队列
list thread_all_list;	    // 所有任务队列
static list_elem* thread_tag;// 用于保存队列中的线程结点
lock pid_lock;             //分配pid的锁

//void switch_to(task_struct* cur, task_struct* next);
extern "C"{
    void switch_to(task_struct* cur, task_struct* next);
}

/* 系统空闲时运行的线程 */
static void idle(void* arg UNUSED) {
   while(1) {
      running_thread()->thread_block(TASK_BLOCKED);//将自己再次加入阻塞队列
         
      //执行hlt时必须要保证目前处在开中断的情况下
      asm volatile ("sti; hlt" : : : "memory");
   }
}

/* 主动让出cpu,换其它线程运行 */
void thread_yield(void) {
   struct task_struct* cur = running_thread();   
   enum intr_status old_status = intr_disable();
   //ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
   thread_ready_list.push_front(&cur->general_tag);
   //list_append(&thread_ready_list, &cur->general_tag);
   cur->status = TASK_READY;
   schedule();
   intr_set_status(old_status);
}

/* 当前线程将自己阻塞,标志其状态为stat. */
void task_struct::thread_block(enum task_status stat) {
/* stat取值为TASK_BLOCKED,TASK_WAITING,TASK_HANGING,也就是只有这三种状态才不会被调度*/
   enum intr_status old_status = intr_disable();
   status = stat; // 置其状态为stat 
   /* 将当前线程换下处理器，注意新上来的线程一定在就绪队列，将他换上来之后他会中断返回，自然会开中断*/
   schedule();	
/* 待当前线程被解除阻塞后才继续运行下面的intr_set_status */
   intr_set_status(old_status);
}

/* 将线程pthread解除阻塞，解除阻塞后是加入就绪队列 */
void task_struct::thread_unblock() {
    enum intr_status old_status = intr_disable();
    if (status != TASK_READY) {
        thread_ready_list.push_back(&general_tag);    // 放到队尾,使其尽快得到调度(我们是链表尾部出队)
        status = TASK_READY;
   } 
   intr_set_status(old_status);
}

/* 分配pid */
static pid_t allocate_pid(void) {
   static pid_t next_pid = 0;
   pid_lock.lock_acquire();
   next_pid++;
   pid_lock.lock_release();
   return next_pid;
}



/* 获取当前线程pcb指针 */
struct task_struct* running_thread() {
   uint32_t esp; 
   asm ("mov %%esp, %0" : "=g" (esp));
  /* 取esp整数部分即pcb起始地址 */
   return (struct task_struct*)(esp & 0xfffff000);
}

/* 由kernel_thread去执行function(func_arg) */
static void kernel_thread(thread_func* function, void* func_arg) {
/* 执行function前要开中断,避免后面的时钟中断被屏蔽,而无法调度其它线程 */
   intr_enable();
   function(func_arg); 
}

/* 初始化线程s所有信息 */
void task_struct::init(char* name, int prio,thread_func function, void* func_arg) {
   memset(this, 0, sizeof(task_struct));//将当前PCB全部清0
   pid = allocate_pid();
   strcpy(this->name, name);

   if (this == main_thread) {
/* 由于把main函数也封装成一个线程,并且它一直是运行的,故将其直接设为TASK_RUNNING */
      status = TASK_RUNNING;
   } else {
      status = TASK_READY;
   }

/* self_kstack是线程自己在内核态下使用的栈顶地址 */
   self_kstack = (uint32_t*)((uint32_t)this + PG_SIZE);
   priority = prio;
   ticks = prio;
   elapsed_ticks = 0;
   pgdir = nullptr;
   stack_magic = 0x19870916;	  // 自定义的魔数
   
   /* 先预留中断使用栈的空间,可见thread.h中定义的结构 */
   self_kstack -= sizeof(intr_stack);

   /* 再留出线程栈空间,可见thread.h中定义 */
   self_kstack -= sizeof(thread_stack);
   thread_stack* kthread_stack = (thread_stack*)self_kstack;

   /* 初始化线程栈 */
   kthread_stack->eip = kernel_thread;
   kthread_stack->function = function;
   kthread_stack->func_arg = func_arg;
   kthread_stack->ebp = kthread_stack->ebx = kthread_stack->esi = kthread_stack->edi = 0;   
}

/* 创建一优先级为prio的线程,线程名为name,线程所执行的函数是function(func_arg) */
task_struct* thread_create(char* name, int prio, thread_func function, void* func_arg) {
/* pcb都位于内核空间,包括用户进程的pcb也是在内核空间 */
   task_struct* thread = (task_struct*)malloc_page(PF_KERNEL,1);

   thread->init(name,prio,function,func_arg);

   thread_ready_list.push_front(&thread->general_tag);

   thread_all_list.push_front(&thread->all_list_tag);

   return thread;
}

/* 将kernel中的main函数完善为主线程 */
static void make_main_thread(void) {
/* 因为main线程早已运行,咱们在loader.S中进入内核时的mov esp,0xc009f000,
就是为其预留了tcb,地址为0xc009e000,因此不需要通过get_kernel_page另分配一页*/
   main_thread = running_thread();
   main_thread->init((char*)"main",31,nullptr,nullptr);
   //init_thread(main_thread, (char*)"main", 31);

/* main函数是当前线程,当前线程不在thread_ready_list中,
 * 所以只将其加在thread_all_list中. */
    thread_all_list.push_front(&main_thread->all_list_tag);
}

/* 实现任务调度 */
void schedule() {

   struct task_struct* cur = running_thread(); 
   if (cur->status == TASK_RUNNING) { // 若此线程只是cpu时间片到了,将其加入到就绪队列尾
      //ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
      //list_append(&thread_ready_list, &cur->general_tag);
      thread_ready_list.push_front(&cur->general_tag);
      cur->ticks = cur->priority;     // 重新将当前线程的ticks再重置为其priority;
      cur->status = TASK_READY;
   } else { 
      /* 若此线程需要某事件发生后才能继续上cpu运行,
      不需要将其加入队列,因为当前线程不在就绪队列中。*/
   }

   thread_tag = nullptr;	  // thread_tag清空
//    k_printf("readylist remain %d \n",thread_ready_list.size);
//    if(thread_ready_list.size == 0){
//        k_printf("ready list is empty!!!!!!!!\n");
//    }
/* 将thread_ready_list队列中的第一个就绪线程弹出,准备将其调度上cpu. */
   if(thread_ready_list.is_empty()){//如果就绪队列为空，将idle_thread线程唤醒
      idle_thread->thread_unblock();
   }
   thread_tag = thread_ready_list.pop_back();   
   struct task_struct* next = elem2entry(struct task_struct, general_tag, thread_tag);
   next->status = TASK_RUNNING;

   /* 击活任务页表等 */
   process_activate(next);

   switch_to(cur, next);
}

/* 初始化线程环境 */
void thread_init(void) {
   k_printf("thread_init start\n");
   thread_ready_list.init();
   thread_all_list.init();
   pid_lock.init();
/* 将当前main函数创建为线程 */
   make_main_thread();
   idle_thread = thread_create("idle",10,idle,nullptr);//初始化idle线程
   k_printf("thread_init done\n");
}

