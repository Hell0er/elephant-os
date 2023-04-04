#ifndef __DEVICE_IOQUEUE_H
#define __DEVICE_IOQUEUE_H
#include "stdint.h"
#include "thread.h"
#include "sync.h"

#define bufsize 64

/* 环形队列 */
struct ioqueue{
    // 生产者消费者问题
    struct lock lock;
    /* 生产者，缓冲区不满就可以继续往里面写数据；
     * 否则就睡眠，等待消费者消费后唤醒 */
    struct task_struct* producer;

    /* 消费者，缓冲区不空就可以继续从里面读数据；
     * 否则就睡眠，等待生产者生产后唤醒 */
    struct task_struct* consumer;

    char buf[bufsize];   // 缓冲区
    int32_t head;        // 队头，从此处写
    int32_t tail;        // 队尾，从此处读
};

void ioqueue_init(struct ioqueue* ioq);
bool ioq_full(struct ioqueue* ioq);
bool ioq_empty(struct ioqueue* ioq);
char ioq_getchar(struct ioqueue* ioq);
void ioq_putchar(struct ioqueue* ioq,char byte);
uint32_t ioq_length(struct ioqueue* ioq);

#endif

