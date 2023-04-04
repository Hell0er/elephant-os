#ifndef __THREAD_SYNC_H
#define __THREAD_SYNC_H
#include "list.h"
#include "stdint.h"
#include "thread.h"

struct semaphore{
    uint8_t value;                // 信号量的值 
    struct list waiters;          // 等待该信号量的线程的队列
};

struct lock{
    struct task_struct* holder;   // 锁的持有者
    struct semaphore semaphore;   // 信号量
    uint32_t holder_repeat_nr;    // 持有者重复申请锁的次数
};

void sema_init(struct semaphore* psema,uint8_t value);
void lock_init(struct lock* lock);
void sema_down(struct semaphore* psema);
void sema_up(struct semaphore* psema);
void lock_acquire(struct lock* plock);
void lock_release(struct lock* plock);
#endif

