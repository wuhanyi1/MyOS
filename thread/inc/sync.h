#ifndef __THREAD_SYNC_H
#define __THREAD_SYNC_H
#include "list.h"
#include "stdint.h"

class task_struct;
/* 记录型信号量结构 */
struct semaphore {
    int8_t  value; //资源的数目,注意我们后面有PV操作要把他变成负数，别整成无符号类型了
    list waiters;   //资源的阻塞队列
    void init(int8_t value); 
    //分别为PV操作，这些操作必须是原子的(用中断保证原子)
    void sema_wait();
    void sema_post();
};

/* 锁结构 */
struct lock {
    struct task_struct* holder;	    // 锁的持有者
    semaphore sem;	    // 用二元信号量实现锁
    uint32_t holder_repeat_nr;		    // 锁的持有者重复申请锁的次数

    void init();
    void lock_acquire();
    void lock_release();
};



#endif
