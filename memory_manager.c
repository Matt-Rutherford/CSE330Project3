#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
//#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
//#include <linux/semaphore.h> probably won't need these two
//#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/mm_types.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <asm/pgtable.h>
#include <asm/mmu_context.h>

int pid;

module_param(pid, int, 0644);

struct task_struct* task;
struct vm_area_struct* vmas;

unsigned long timer_interval_ns = 10e9; // call every 10 sec
static struct hrtimer hr_timer;

pgd_t* pgd;
p4d_t* p4d;
pud_t* pud;
pmd_t* pmd;
pte_t* ptep, pte;

unsigned long wss = 0; // working set size
unsigned long x;       // addr

// -----------------------------------------------------------------------------------
void TraversePageTable(bool mode) {

    vmas = task->mm->mmap; // points to virtual memory area space
	
    while(vmas) { // traverse the virtual address spaces
       
        for(x = vmas->vm_start; x <= (vmas->vm_end-PAGE_SIZE); x+= PAGE_SIZE) { // traverse the pages in task's virtual memory area
            pgd = pgd_offset(task->mm,x);
            p4d = p4d_offset(pgd,x);
            pud = pud_offset(p4d,x);
            pmd = pmd_offset(pud,x);
            ptep = pte_offset_map(pmd,x);
           
			mmap_read_lock(task->mm); // lock the page to read it

			if(mode == true ) { // clear accessed bit
				pte_mkold(*ptep);
			} else { // count the WSS
				if( pte_young(*ptep) ) {
					wss = wss + 1;
				}
			}
			
			//ptep_test_and clear_youg(vma, x, ptep);

			mmap_read_unlock(task->mm); // unlock page from read lock	
		}
		
		vmas = vmas->vm_next; // get next virtual address space 
	}
}

// -----------------------------------------------------------------------------------
//test and clear the accessed bit of the given pte_t entry
int ptep_test_and_clear_young(struct vm_area_struct *vma, unsigned long addr, pte_t *ptep) {
	int ret = 0;
	
	if (pte_young(*ptep))
		ret = test_and_clear_bit(_PAGE_BIT_ACCESSED, (unsigned long *) &ptep-pte);
	
	return ret;
}
// -----------------------------------------------------------------------------------
// called after 10 sec
enum hrtimer_restart timer_restart_callback(struct hrtimer *timer) { 
    ktime_t currtime , interval;
    currtime = ktime_get();
    interval = ktime_set(0, timer_interval_ns);
    hrtimer_forward(timer, currtime, interval);

	for_each_process(task) {
        if(task->pid == pid) {
            goto traverse; // escape loop and traverse the 5-level page table
        }
    }
	
	traverse:
		TraversePageTable(false);
		printk("[%d]:[%lu]", pid, wss);
		wss = 0;
		TraversePageTable(true);

    return HRTIMER_NORESTART; // used to invoke the HRT periodically
}

// -----------------------------------------------------------------------------------
int producer_consumer_init(void) { 
	printk("Init start");
    ktime_t ktime;
	
	ktime = ktime_set(0, timer_interval_ns); 
    hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    hr_timer.function = &timer_restart_callback;
    hrtimer_start(&hr_timer, ktime, HRTIMER_MODE_REL);

    return 0;

}
// -----------------------------------------------------------------------------------
void producer_consumer_exit(void) {
    int ret;
    ret = hrtimer_cancel(&hr_timer);
    if(ret) {
        printk("Timer still running.\n");
    }
    printk("HR Timer removed\n");
}


MODULE_LICENSE("GPL");

module_init(producer_consumer_init);
module_exit(producer_consumer_exit);