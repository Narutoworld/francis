/*
 * Core thread system.
 */
#include <types.h>
#include <lib.h>
#include <kern/errno.h>
#include <array.h>
#include <machine/spl.h>
#include <machine/pcb.h>
#include <thread.h>
#include <curthread.h>
#include <scheduler.h>
#include <addrspace.h>
#include <vnode.h>
#include <machine/trapframe.h>
#include "opt-synchprobs.h"
#include "elf.h"

//global process table
extern struct process* processTable[256];

void process_table_bootstrap(){
	int i;
	for(i = 1; i<256; i++){
		processTable[i] = NULL;
	}
	return;
}

void process_table_shutdown(){
	int i;
	for(i = 1; i<256; i++){
		kfree(processTable[i]);
	}
	return;
}

//create a new process
struct process* process_create(pid_t parentid, struct thread* thread){
	//create a new process	
	struct process* newprocess = kmalloc(sizeof(struct process));
	//memory is out of space
	if(newprocess == NULL){
		return NULL;	
	}
	//assign the value

	newprocess->parentPid = parentid;
	newprocess->mythread = thread;
	newprocess->exit_wait_lock = sem_create("exit_wait_lock", 0);
	newprocess->exit_code = 0;

	return newprocess;

}

//assign a process id for new created process
int assign_pid(int* childpid, struct thread* thread){
	

	struct process* newpro;
	
	
	//find if there is a null space in the processTable(previously occupied by other process but it is deleted)
	int i;
	int nullspaceid = -1;
	for(i = 1; i<256; i++){
		if(processTable[i] == NULL){
			nullspaceid = i;
			break;
		}
	}
	//check if process id had exceeded the maximum number
	if(nullspaceid == -1){
		return EAGAIN;
	}
	
	//found a null space in processTable
	else{
		newpro = process_create(curthread->processid, thread);
		processTable[nullspaceid] = newpro;
		*childpid = nullspaceid;
		return 0;
	}
	
	
}

void freeprocess(int pid) {

    //delete the element in this process
    sem_destroy(processTable[pid]->exit_wait_lock);
    //    	if (processTable[pid]->mythread->t_vmspace) {
    //		/*
    //		 * Do this carefully to avoid race condition with
    //		 * context switch code.
    //		 */
    //		struct addrspace *as = processTable[pid]->mythread->t_vmspace;
    //		processTable[pid]->mythread->t_vmspace = NULL;
    //		as_destroy(as);
    //	}
    //    	if (processTable[pid]->mythread->t_cwd) {
    //		VOP_DECREF(processTable[pid]->mythread->t_cwd);
    //		processTable[pid]->mythread->t_cwd = NULL;
    //	}
    //        thread_destroy(processTable[pid]->mythread);
    //processTable[pid]->mythread = NULL;
    kfree(processTable[pid]);
    processTable[pid] = NULL;

    return;

}
int first_coremap_page_num;
int max_coremap_entries;
int num_coremap_free;

void coremap_print(){
    int i;
    kprintf("----------------------");
    for (i = 0; i < max_coremap_entries; i++){
//        if (coremapTable[i].status != 'H' && coremapTable[i].status != 'P'){
//            kprintf("fuck ni ma");
//        }
        if (i % 10 == 0)
            kprintf("|\n| ");
//        kprintf("startAddr: %x: ", coremapTable[i].startAddr);
        kprintf("%c ", coremapTable[i].status);
    }
    kprintf("\n----------------------");
}




void coremap_table_bootstrap(void) {
    u_int32_t coremapsize;
    u_int32_t num_of_page;
    u_int32_t i;

    /* get available physical frame*/
    ram_getsize(&firstpaddr, &lastpaddr);
    num_of_page = (lastpaddr - firstpaddr) / PAGE_SIZE;
    //        kprintf ("\nfirstpaddr: %d, lastpaddr: %d\n",firstpaddr, lastpaddr);

    /*coremap size*/
    coremapsize = num_of_page * sizeof (struct coremap);

    first_coremap_page_num = firstpaddr / PAGE_SIZE;
    max_coremap_entries = ((lastpaddr - firstpaddr) / PAGE_SIZE) - 4;
    num_coremap_free = max_coremap_entries;

    /* set global coremap paremeters*/
    coremapTable = (struct coremap *) PADDR_TO_KVADDR(firstpaddr);

    /* initialize coremap*/
    for (i = 0; i < max_coremap_entries; i++) {
        coremapTable[i].status = 'H';
        coremapTable[i].selfAddr = firstpaddr;
        coremap_self[i] = firstpaddr;
        coremapTable[i].as = NULL;
        coremapTable[i].startAddr = (first_coremap_page_num + i) * PAGE_SIZE;
        coremapTable[i].length = 0;
        coremapTable[i].start = 0;
        coremapTable[i].fixed = 0;
        coremapTable[i].referenced = 0;
        coremapTable[i].counter = 0;
        coremapTable[i].modified = 0;
        coremapTable[i].copy = 0;
        coremapTable[i].first_level_entry = -1;
        coremapTable[i].second_level_entry = -1;
        firstpaddr += coremapsize;
    }
    
    /*make the address for coremapTable untouchable*/
    int cormap_reserved_size;
    cormap_reserved_size = sizeof (coremapTable) / PAGE_SIZE;
    if ((sizeof(coremapTable)) % PAGE_SIZE != 0)
        cormap_reserved_size++;
    for (i = 0; i < cormap_reserved_size; i++) {
        coremapTable[i].status = 'P';
        num_coremap_free -= 1;
    }
    
    
}

int get_page(int npages) {
    int i;
    int selected_page_num;
    
    /*if no enough page frame this time*/
    if (num_coremap_free < npages)
        return -1;
    
    /*if only one page needed*/
    if (npages == 1) {
        for (i = 0; i < max_coremap_entries; i++) {
            if (coremapTable[i].status == 'H') {
                selected_page_num = i;
                break;
            }
        }
        return selected_page_num;
    } else {
        
        /*if more than one page needed*/
        selected_page_num = -1;
        
        /*searching for n continuous holes*/
        for (i = 0; i < max_coremap_entries; i++) {
            
            /*got the first index of the n continuous holes, check for the rest*/
            if (coremapTable[i].status == 'H') {
                int j;
                selected_page_num = -1;
                for (j = i; j < i + npages; j++) {
                    
                    /*if one page frame is being used at the middle of this check, search for a new potential place*/
                    if (coremapTable[j].status == 'P')
                        break;
                    if (j == i + npages - 1)
                        selected_page_num = i;
                }
                
                /*we get our candidate!*/
                if (selected_page_num == i)
                    break;
            }
        }
        return selected_page_num;
    }
}
/* States a thread can be in. */
typedef enum {
	S_RUN,
	S_READY,
	S_SLEEP,
	S_ZOMB,
} threadstate_t;

/* Global variable for the thread currently executing at any given time. */
struct thread *curthread;

/* Table of sleeping threads. */
static struct array *sleepers;

/* List of dead threads to be disposed of. */
static struct array *zombies;

/* Total number of outstanding threads. Does not count zombies[]. */
static int numthreads;


int one_thread_only() {
  int s;
  int n;
  /* numthreads is a shared variable, so turn interrupts
     off to ensure that we can inspect its value atomically */
  s = splhigh();
  n = numthreads;
  splx(s);
  return(n==1);
}


/*
 * Create a thread. This is used both to create the first thread's 
 * thread structure and to create subsequent threads.
 */

static
struct thread *
thread_create(const char *name)
{
	struct thread *thread = kmalloc(sizeof(struct thread));
	if (thread==NULL) {
		return NULL;
	}
	thread->t_name = kstrdup(name);
	if (thread->t_name==NULL) {
		kfree(thread);
		return NULL;
	}
	thread->t_sleepaddr = NULL;
	thread->t_stack = NULL;
	
	thread->t_vmspace = NULL;

	thread->t_cwd = NULL;
        
	thread->processid = 0;
	// If you add things to the thread structure, be sure to initialize
	
	// them here.
//        kprintf("in thread_create address space : %x, thread : %x\n", thread->t_vmspace, thread);
	return thread;
}

/*
 * Destroy a thread.
 *
 * This function cannot be called in the victim thread's own context.
 * Freeing the stack you're actually using to run would be... inadvisable.
 */
static
void
thread_destroy(struct thread *thread)
{
	assert(thread != curthread);

	// If you add things to the thread structure, be sure to dispose of
	// them here or in thread_exit.

	// These things are cleaned up in thread_exit.
	assert(thread->t_vmspace==NULL);
	assert(thread->t_cwd==NULL);
	
	if (thread->t_stack) {
		kfree(thread->t_stack);
	}

	kfree(thread->t_name);
	kfree(thread);
}


/*
 * Remove zombies. (Zombies are threads/processes that have exited but not
 * been fully deleted yet.)
 */
static
void
exorcise(void)
{
	int i, result;

	assert(curspl>0);
	
	for (i=0; i<array_getnum(zombies); i++) {
		struct thread *z = array_getguy(zombies, i);
		assert(z!=curthread);
		thread_destroy(z);
	}
	result = array_setsize(zombies, 0);
	/* Shrinking the array; not supposed to be able to fail. */
	assert(result==0);
}

/*
 * Kill all sleeping threads. This is used during panic shutdown to make 
 * sure they don't wake up again and interfere with the panic.
 */
static
void
thread_killall(void)
{
	int i, result;

	assert(curspl>0);

	/*
	 * Move all sleepers to the zombie list, to be sure they don't
	 * wake up while we're shutting down.
	 */

	for (i=0; i<array_getnum(sleepers); i++) {
		struct thread *t = array_getguy(sleepers, i);
		kprintf("sleep: Dropping thread %s\n", t->t_name);

		/*
		 * Don't do this: because these threads haven't
		 * been through thread_exit, thread_destroy will
		 * get upset. Just drop the threads on the floor,
		 * which is safer anyway during panic.
		 *
		 * array_add(zombies, t);
		 */
	}

	result = array_setsize(sleepers, 0);
	/* shrinking array: not supposed to fail */
	assert(result==0);
}

/*
 * Shut down the other threads in the thread system when a panic occurs.
 */
void
thread_panic(void)
{
	assert(curspl > 0);

	thread_killall();
	scheduler_killall();
}

/*
 * Thread initialization.
 */
struct thread *
thread_bootstrap(void)
{
	struct thread *me;

	/* Create the data structures we need. */
	sleepers = array_create();
	if (sleepers==NULL) {
		panic("Cannot create sleepers array\n");
	}

	zombies = array_create();
	if (zombies==NULL) {
		panic("Cannot create zombies array\n");
	}
	
	/*
	 * Create the thread structure for the first thread
	 * (the one that's already running)
	 */
	me = thread_create("<boot/menu>");
	if (me==NULL) {
		panic("thread_bootstrap: Out of memory\n");
	}

	/*
	 * Leave me->t_stack NULL. This means we're using the boot stack,
	 * which can't be freed.
	 */

	/* Initialize the first thread's pcb */
	md_initpcb0(&me->t_pcb);

	/* Set curthread */
	curthread = me;

	/* Number of threads starts at 1 */
	numthreads = 1;

	/* Done */
	return me;
}

/*
 * Thread final cleanup.
 */
void
thread_shutdown(void)
{
	array_destroy(sleepers);
	sleepers = NULL;
	array_destroy(zombies);
	zombies = NULL;
	// Don't do this - it frees our stack and we blow up
	//thread_destroy(curthread);
}

/*
 * Create a new thread based on an existing one.
 * The new thread has name NAME, and starts executing in function FUNC.
 * DATA1 and DATA2 are passed to FUNC.
 */
int
thread_fork(const char *name, 
	    void *data1, unsigned long data2,
	    void (*func)(void *, unsigned long),
	    struct thread **ret)
{
//        kprintf("in thread_fork address space : %x, curthread : %x\n", curthread->t_vmspace, curthread);
	struct thread *newguy;
	int s, result;

	/* Allocate a thread */
	newguy = thread_create(name);
	if (newguy==NULL) {
		return ENOMEM;
	}

	/* Allocate a stack */
	newguy->t_stack = kmalloc(STACK_SIZE);
	if (newguy->t_stack==NULL) {
		kfree(newguy->t_name);
		kfree(newguy);
		return ENOMEM;
	}

	/* stick a magic number on the bottom end of the stack */
	newguy->t_stack[0] = 0xae;
	newguy->t_stack[1] = 0x11;
	newguy->t_stack[2] = 0xda;
	newguy->t_stack[3] = 0x33;

	/* Inherit the current directory */
	if (curthread->t_cwd != NULL) {
		VOP_INCREF(curthread->t_cwd);
		newguy->t_cwd = curthread->t_cwd;
	}

	/* Set up the pcb (this arranges for func to be called) */
	md_initpcb(&newguy->t_pcb, newguy->t_stack, data1, data2, func);
	pid_t tempchildid;	
	result = assign_pid(&tempchildid, newguy);
	newguy->processid = tempchildid;
	/* Interrupts off for atomicity */
	s = splhigh();

	/*
	 * Make sure our data structures have enough space, so we won't
	 * run out later at an inconvenient time.
	 */
	result = array_preallocate(sleepers, numthreads+1);
	if (result) {
		goto fail;
	}
	result = array_preallocate(zombies, numthreads+1);
	if (result) {
		goto fail;
	}

	/* Do the same for the scheduler. */
	result = scheduler_preallocate(numthreads+1);
	if (result) {
		goto fail;
	}

	/* Make the new thread runnable */
	result = make_runnable(newguy);
	if (result != 0) {
		goto fail;
	}

	/*
	 * Increment the thread counter. This must be done atomically
	 * with the preallocate calls; otherwise the count can be
	 * temporarily too low, which would obviate its reason for
	 * existence.
	 */
	numthreads++;

	/* Done with stuff that needs to be atomic */
	splx(s);

	/*
	 * Return new thread structure if it's wanted.  Note that
	 * using the thread structure from the parent thread should be
	 * done only with caution, because in general the child thread
	 * might exit at any time.
	 */
	if (ret != NULL) {
		*ret = newguy;
	}

	return 0;

 fail:
	splx(s);
	if (newguy->t_cwd != NULL) {
		VOP_DECREF(newguy->t_cwd);
	}
	kfree(newguy->t_stack);
	kfree(newguy->t_name);
	kfree(newguy);

	return result;
}

/*
 * High level, machine-independent context switch code.
 */
static
void
mi_switch(threadstate_t nextstate)
{
	struct thread *cur, *next;
	int result;
	
	/* Interrupts should already be off. */
	assert(curspl>0);

	if (curthread != NULL && curthread->t_stack != NULL) {
		/*
		 * Check the magic number we put on the bottom end of
		 * the stack in thread_fork. If these assertions go
		 * off, it most likely means you overflowed your stack
		 * at some point, which can cause all kinds of
		 * mysterious other things to happen.
		 */
		assert(curthread->t_stack[0] == (char)0xae);
		assert(curthread->t_stack[1] == (char)0x11);
		assert(curthread->t_stack[2] == (char)0xda);
		assert(curthread->t_stack[3] == (char)0x33);
	}
	
	/* 
	 * We set curthread to NULL while the scheduler is running, to
	 * make sure we don't call it recursively (this could happen
	 * otherwise, if we get a timer interrupt in the idle loop.)
	 */
	if (curthread == NULL) {
		return;
	}
	cur = curthread;
	curthread = NULL;

	/*
	 * Stash the current thread on whatever list it's supposed to go on.
	 * Because we preallocate during thread_fork, this should not fail.
	 */

	if (nextstate==S_READY) {
		result = make_runnable(cur);
	}
	else if (nextstate==S_SLEEP) {
		/*
		 * Because we preallocate sleepers[] during thread_fork,
		 * this should never fail.
		 */
		result = array_add(sleepers, cur);
	}
	else {
		assert(nextstate==S_ZOMB);
		result = array_add(zombies, cur);
	}
	assert(result==0);

	/*
	 * Call the scheduler (must come *after* the array_adds)
	 */

	next = scheduler();

	/* update curthread */
	curthread = next;
	
	/* 
	 * Call the machine-dependent code that actually does the
	 * context switch.
	 */
	md_switch(&cur->t_pcb, &next->t_pcb);
	
	/*
	 * If we switch to a new thread, we don't come here, so anything
	 * done here must be in mi_threadstart() as well, or be skippable,
	 * or not apply to new threads.
	 *
	 * exorcise is skippable; as_activate is done in mi_threadstart.
	 */

	exorcise();

	if (curthread->t_vmspace) {
		as_activate(curthread->t_vmspace);
	}
}

/*
 * Cause the current thread to exit.
 *
 * We clean up the parts of the thread structure we don't actually
 * need to run right away. The rest has to wait until thread_destroy
 * gets called from exorcise().
 */
void
thread_exit(void)
{
	if (curthread->t_stack != NULL) {
		/*
		 * Check the magic number we put on the bottom end of
		 * the stack in thread_fork. If these assertions go
		 * off, it most likely means you overflowed your stack
		 * at some point, which can cause all kinds of
		 * mysterious other things to happen.
		 */
		assert(curthread->t_stack[0] == (char)0xae);
		assert(curthread->t_stack[1] == (char)0x11);
		assert(curthread->t_stack[2] == (char)0xda);
		assert(curthread->t_stack[3] == (char)0x33);
	}

	splhigh();

	if (curthread->t_vmspace) {
		/*
		 * Do this carefully to avoid race condition with
		 * context switch code.
		 */
		struct addrspace *as = curthread->t_vmspace;
                as_destroy(as);
		curthread->t_vmspace = NULL;		
	}

	if (curthread->t_cwd) {
		VOP_DECREF(curthread->t_cwd);
		curthread->t_cwd = NULL;
	}

	assert(numthreads>0);
	numthreads--;
	mi_switch(S_ZOMB);

	panic("Thread came back from the dead!\n");
}

/*
 * Yield the cpu to another process, but stay runnable.
 */
void
thread_yield(void)
{
	int spl = splhigh();

	/* Check sleepers just in case we get here after shutdown */
	assert(sleepers != NULL);

	mi_switch(S_READY);
	splx(spl);
}

/*
 * Yield the cpu to another process, and go to sleep, on "sleep
 * address" ADDR. Subsequent calls to thread_wakeup with the same
 * value of ADDR will make the thread runnable again. The address is
 * not interpreted. Typically it's the address of a synchronization
 * primitive or data structure.
 *
 * Note that (1) interrupts must be off (if they aren't, you can
 * end up sleeping forever), and (2) you cannot sleep in an 
 * interrupt handler.
 */
void
thread_sleep(const void *addr)
{
	// may not sleep in an interrupt handler
	assert(in_interrupt==0);
	
	curthread->t_sleepaddr = addr;
	mi_switch(S_SLEEP);
	curthread->t_sleepaddr = NULL;
}

/*
 * Wake up one or more threads who are sleeping on "sleep address"
 * ADDR.
 */
void
thread_wakeup(const void *addr)
{
	int i, result;
	
	// meant to be called with interrupts off
	assert(curspl>0);
	
	// This is inefficient. Feel free to improve it.
	
	for (i=0; i<array_getnum(sleepers); i++) {
		struct thread *t = array_getguy(sleepers, i);
		if (t->t_sleepaddr == addr) {
			
			// Remove from list
			array_remove(sleepers, i);
			
			// must look at the same sleepers[i] again
			i--;

			/*
			 * Because we preallocate during thread_fork,
			 * this should never fail.
			 */
			result = make_runnable(t);
			assert(result==0);
		}
	}
}



void
thread_wakeup_one(const void *addr)
{
	int i, result;

	assert(curspl>0);

	for(i = 0; i<array_getnum(sleepers); i++) {
		struct thread *t = array_getguy(sleepers, i);
		if (t->t_sleepaddr == addr) {
			array_remove(sleepers, i);
			result = make_runnable(t);
			assert (result==0);
			break;
		}
	}
}






/*
 * Return nonzero if there are any threads who are sleeping on "sleep address"
 * ADDR. This is meant to be used only for diagnostic purposes.
 */
int
thread_hassleepers(const void *addr)
{
	int i;
	
	// meant to be called with interrupts off
	assert(curspl>0);
	
	for (i=0; i<array_getnum(sleepers); i++) {
		struct thread *t = array_getguy(sleepers, i);
		if (t->t_sleepaddr == addr) {
			return 1;
		}
	}
	return 0;
}

/*
 * New threads actually come through here on the way to the function
 * they're supposed to start in. This is so when that function exits,
 * thread_exit() can be called automatically.
 */
void
mi_threadstart(void *data1, unsigned long data2, 
	       void (*func)(void *, unsigned long))
{
	/* If we have an address space, activate it */
	if (curthread->t_vmspace) {
		as_activate(curthread->t_vmspace);
	}

	/* Enable interrupts */
	spl0();

#if OPT_SYNCHPROBS
	/* Yield a random number of times to get a good mix of threads */
	{
		int i, n;
		n = random()%161 + random()%161;
		for (i=0; i<n; i++) {
			thread_yield();
		}
	}
#endif
	
	/* Call the function */
	func(data1, data2);

	/* Done. */
	thread_exit();
}

pid_t
process_fork(const char *name, 
	    void *data1, int *idc,
	    void (*func)(void *, unsigned long),
	    struct thread **ret)
{
	struct thread *newguy;
	int s, result;
		

	/* Allocate a thread */
	newguy = thread_create(name);
	if (newguy==NULL) {
		return ENOMEM;
	}

	/* Allocate a stack */
	newguy->t_stack = kmalloc(STACK_SIZE);
	if (newguy->t_stack==NULL) {
		kfree(newguy->t_name);
		kfree(newguy);
		return ENOMEM;
	}

	/* stick a magic number on the bottom end of the stack */
	newguy->t_stack[0] = 0xae;
	newguy->t_stack[1] = 0x11;
	newguy->t_stack[2] = 0xda;
	newguy->t_stack[3] = 0x33;

	/* Inherit the current directory */
	if (curthread->t_cwd != NULL) {
		VOP_INCREF(curthread->t_cwd);
		//newguy->t_cwd = curthread->t_cwd;
		memcpy(newguy->t_cwd, curthread->t_cwd, sizeof(struct vnode));
	}

	//copy virtual memory
	if(curthread->t_vmspace != NULL){
		int ret = as_copy(curthread->t_vmspace, &newguy->t_vmspace);
		if(ret != 0){
			kfree(newguy->t_name);
			kfree(newguy->t_stack);
			kfree(newguy->t_cwd);
			kfree(newguy);
			return ENOMEM;		
		}
	}

	//copy sleep address
	newguy->t_sleepaddr = curthread->t_sleepaddr;

	//copy trapframe
	struct trapframe* tf_child = kmalloc(sizeof(struct trapframe));
	memcpy(tf_child, (struct trapframe*)data1, sizeof(struct trapframe));
	
	//initialize pcb for child process
	md_initpcb(&newguy->t_pcb, newguy->t_stack, tf_child, newguy->t_vmspace, func);

	//assign a new process id for new created child process/////////////////////////////////////
	pid_t tempchildid;	
	int res = assign_pid(&tempchildid, newguy);
	newguy->processid = tempchildid;
	*idc = tempchildid;

	/* Set up the pcb (this arranges for func to be called) */
	//md_initpcb(&newguy->t_pcb, newguy->t_stack, data1, data2, func);

	/* Interrupts off for atomicity */
	s = splhigh();

	/*
	 * Make sure our data structures have enough space, so we won't
	 * run out later at an inconvenient time.
	 */
	result = array_preallocate(sleepers, numthreads+1);
	if (result) {
		goto fail;
	}
	result = array_preallocate(zombies, numthreads+1);
	if (result) {
		goto fail;
	}

	/* Do the same for the scheduler. */
	result = scheduler_preallocate(numthreads+1);
	if (result) {
		goto fail;
	}

	/* Make the new thread runnable */
	result = make_runnable(newguy);
	if (result != 0) {
		goto fail;
	}

	/*
	 * Increment the thread counter. This must be done atomically
	 * with the preallocate calls; otherwise the count can be
	 * temporarily too low, which would obviate its reason for
	 * existence.
	 */
	numthreads++;

	/* Done with stuff that needs to be atomic */
	splx(s);

	/*
	 * Return new thread structure if it's wanted.  Note that
	 * using the thread structure from the parent thread should be
	 * done only with caution, because in general the child thread
	 * might exit at any time.
	 */
	if (ret != NULL) {
		*ret = newguy;
	}

	return 0;

 fail:
	splx(s);
	if (newguy->t_cwd != NULL) {
		VOP_DECREF(newguy->t_cwd);
	}
	kfree(newguy->t_stack);
	kfree(newguy->t_name);
	kfree(newguy);

	return result;
}

