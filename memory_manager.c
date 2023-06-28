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

unsigned int page_walk_counter;
unsigned long wss = 0; // working set size
unsigned long rss = 0;
unsigned long swap = 0;
unsigned long x;       // addr

// -----------------------------------------------------------------------------------
//test and clear the accessed bit of the given pte_t entry
int ptep_test_and_clear_young(struct vm_area_struct *vma, unsigned long addr, pte_t *ptep) {
	int ret = 0;
	
	if (pte_young(*ptep))
		ret = test_and_clear_bit(_PAGE_BIT_ACCESSED, (unsigned long *) &ptep->pte);
	
	return ret;
}

// -----------------------------------------------------------------------------------
void TraversePageTable(void) {

    vmas = task->mm->mmap; // points to virtual memory area space
	//rss = task->mm->hiwater_rss;
    while(vmas) { // traverse the virtual address spaces
       //PAGE_SIZE = 4096
        
        for(x = vmas->vm_start; x <= (vmas->vm_end-PAGE_SIZE); x+= PAGE_SIZE) { // traverse the pages in task's virtual memory area
		
            mmap_read_lock(task->mm); // lock the page to read it
			
            pgd = pgd_offset(task->mm, x); // get pgd from mm and the page address
            if (pgd_none(*pgd) || pgd_bad(*pgd)){ // check if pgd is bad or does not exist
                return;}
				
            p4d = p4d_offset(pgd, x); // get p4d from from pgd and the page address
            if (p4d_none(*p4d) || p4d_bad(*p4d)){ // check if p4d is bad or does not exist
                return;}
				
            pud = pud_offset(p4d, x); // get pud from from p4d and the page address
            if (pud_none(*pud) || pud_bad(*pud)){ // check if pud is bad or does not exist
                return;}
				
            pmd = pmd_offset(pud, x); // get pmd from from pud and the page address
            if (pmd_none(*pmd) || pmd_bad(*pmd)){ // check if pmd is bad or does not exist
                return;}
				
            ptep = pte_offset_map(pmd, x); // get pte from pmd and the page address

            if (!ptep){
                return;} // check if pte does not exist
	    pte = *ptep;

            if (pte_present(*ptep)){ //not sure if this is right
                    wss = wss + 1;
                    if(ptep_test_and_clear_young(vmas, x, ptep)){
                       rss++;
                    }
	    } else {
                swap++;
            }
			
            pte_unmap(ptep);
            mmap_read_unlock(task->mm); // unlock page from read lock	

		}
		
		vmas = vmas->vm_next; // get next virtual address space 
	}
}


// -----------------------------------------------------------------------------------
// called after 10 sec
enum hrtimer_restart timer_restart_callback(struct hrtimer *timer) { 
    //uint64_t rawtime;
    ktime_t currtime , interval;

    page_walk_counter++;
    if (page_walk_counter >= 6) {
        return HRTIMER_NORESTART;
    }

    currtime = ktime_get();
    interval = ktime_set(0, timer_interval_ns);
    hrtimer_forward(timer, currtime, interval);

    task = pid_task(find_vpid(pid),PIDTYPE_PID);
	TraversePageTable();
	printk("PID [%d]: RSS = [%lu] KB, SWAP = [%lu] KB, WSS = [%lu] KB", pid, rss*4, swap, wss*4);

    return HRTIMER_RESTART; // used to invoke the HRT periodically
}

// -----------------------------------------------------------------------------------
int producer_consumer_init(void) { 
    ktime_t ktime;

    printk("Init start");
    ktime = ktime_set(0, timer_interval_ns);
    hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    hr_timer.function = &timer_restart_callback;
    hrtimer_start(&hr_timer, ktime, HRTIMER_MODE_REL);

    return 0;

}
// -----------------------------------------------------------------------------------
void producer_consumer_exit(void) {
    //printk("exit");
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
