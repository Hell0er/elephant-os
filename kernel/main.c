#include "print.h"
#include "init.h"
#include "thread.h"
#include "interrupt.h"
#include "console.h"
#include "ioqueue.h"
#include "process.h"
#include "syscall.h"
#include "syscall-init.h"
#include "stdio.h"
#include "memory.h"
#include "fs.h"
#include "string.h"
#include "file.h"
#include "shell.h"
#include "debug.h"
#include "ide.h"
#include "stdio-kernel.h"

void init(void);

int main(void){
    put_str("Welcome,\nI am kernel!\n");
    init_all();
    intr_enable();

    uint32_t file_size=10488;
    uint32_t sec_cnt=DIV_ROUND_UP(file_size,512);
    struct disk* sda=&channels[0].devices[0];
    void* prog_buf=sys_malloc(file_size);
    ide_read(sda,300,prog_buf,sec_cnt);
    int32_t fd=sys_open("/cat",O_CREAT|O_RDWR);
    if (fd!=-1){
        if (sys_write(fd,prog_buf,file_size)==-1){
            printk("file write error!\n");
            while (1);
        }
    }

    // cls_screen();
    console_put_str("[zbb@LAPTOP //]$ ");
    thread_exit(running_thread(),true);
    while(1);
    return 0;
}

/* init进程 */
void init(void){
    uint32_t ret_pid=fork();
    if (ret_pid){   // 父进程
        int status;
    int child_pid;
    /* init在此处不停地回收僵尸进程 */
    while (1){
        child_pid=wait(&status);
        printf("I'm init, My pid is 1, I receive a child. It's pid is %d, status is %d\n",child_pid,status);
    }
    }else {   // 子进程
        my_shell();
    }
    PANIC("init: should not be here.");
}
