#include <linux/module.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/mm_types.h>

int pid;

module_param(pid, int, 0644);

struct task_struct* task;
struct vm_area_struct *mmap;

int producer_consumer_init(void){

    //task = find_task_by_pid(pid);//????? instead of for each process?

    for_each_process(task){
        if (pid == task->pid){
            mmap = task->mm->mmap; //list of vmas
            printk("found PID");
        }
        
    }
    return 0;
}

void producer_consumer_exit(void){
    printk(KERN_INFO "CSE330 Project 3 Kernel Module Removed\n");
}

module_init(producer_consumer_init);
module_exit(producer_consumer_exit);
MODULE_LICENSE("GPL");