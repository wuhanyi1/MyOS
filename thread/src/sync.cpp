#include "sync.h"
#include "list.h"
#include "global.h"
#include "debug.h"
#include "interrupt.h"
#include "k_printf.h"
#include "thread.h"
extern list thread_ready_list,thread_all_list;

/* 初始化信号量 */
void semaphore::init(int8_t value) {
   this->value = value;       // 为信号量赋初值
   waiters.init(); //初始化信号量的等待队列
}

/* 信号量wait操作 */
void semaphore::sema_wait() {
/* 关中断来保证原子操作 */
    enum intr_status old_status = intr_disable();
    value--;
    //value < 0，说明没有资源了,每次进来
    if(value < 0){
        task_struct *thread = running_thread();
        waiters.push_front(&thread->general_tag);//将进程\线程的PCB加入阻塞队列
        /*进程\线程自我阻塞之后，不在ready_List中，永远不会被调度，除非被唤醒*/
        thread->thread_block(TASK_BLOCKED);//加while可以多一层判断，事实上能回到这个执行流，一定是这个线程能运行了
    }
    //k_printf("readylist remain %d\n",thread_ready_list.size);
    //k_printf("value is %d\n",value);
    //value--;
    //k_printf("%s get the stdout lock \n",running_thread()->name);
    //能出来，说明获得了这个资源，恢复之前的中断状态
/* 恢复之前的中断状态 */
   intr_set_status(old_status);
}

/* 信号量的post操作 */
void semaphore::sema_post() {
/* 关中断,保证原子操作 */
    enum intr_status old_status = intr_disable();
    value++;
    //value <= 0,说明有进程\线程在阻塞队列，唤醒一个。一个线程应该只需要唤醒一个线程就行，不然不能一直困在这里
    if(value <= 0){//waiters.size != 0
        //从阻塞队列弹出一个元素
        task_struct* thread = elem2entry(struct task_struct, general_tag, waiters.pop_back());
        thread->thread_unblock();//解除阻塞
    }
    //value++;
/* 恢复之前的中断状态，能出来一定是没得唤醒 */
    intr_set_status(old_status);
}

/* 初始化锁plock */
void lock::init() {
   holder = (task_struct*)NULL;
   holder_repeat_nr = 0;
   sem.init(1);// 信号量初值为1
}

/* 获取锁plock */
void lock::lock_acquire() {
/* 排除曾经自己已经持有锁但还未将其释放的情况。以免自己把自己堵住了*/
   if (holder != running_thread()) { 
      sem.sema_wait();    // 对这个锁的信号量进行P操作,原子操作
      //k_printf("holder is %s\n",running_thread()->name);
      holder = running_thread();
      holder_repeat_nr = 1;
   } else {
      holder_repeat_nr++;//多次申请只需要增加计数就可以了
   }
}

/* 释放锁plock */
void lock::lock_release() {
   if (holder_repeat_nr > 1) {
      holder_repeat_nr--;
      return;
   }

   holder = (task_struct*)NULL;	   // 把锁的持有者置空放在V操作之前
   holder_repeat_nr = 0;
   sem.sema_post();	   // 信号量的V操作,也是原子操作
}

