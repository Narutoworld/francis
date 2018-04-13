#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <machine/pcb.h>
#include <machine/spl.h>
#include <machine/trapframe.h>
#include <kern/callno.h>
#include <syscall.h>
//#include <unistd.h>
#include <thread.h>
//#include <stdio.h>
#include <clock.h>

#include "addrspace.h"


extern struct thread* curthread;

/*
 * System call handler.
 *
 * A pointer to the trapframe created during exception entry (in
 * exception.S) is passed in.
 *
 * The calling conventions for syscalls are as follows: Like ordinary
 * function calls, the first 4 32-bit arguments are passed in the 4
 * argument registers a0-a3. In addition, the system call number is
 * passed in the v0 register.
 *
 * On successful return, the return value is passed back in the v0
 * register, like an ordinary function call, and the a3 register is
 * also set to 0 to indicate success.
 *
 * On an error return, the error code is passed back in the v0
 * register, and the a3 register is set to 1 to indicate failure.
 * (Userlevel code takes care of storing the error code in errno and
 * returning the value -1 from the actual userlevel syscall function.
 * See src/lib/libc/syscalls.S and related files.)
 *
 * Upon syscall return the program counter stored in the trapframe
 * must be incremented by one instruction; otherwise the exception
 * return code will restart the "syscall" instruction and the system
 * call will repeat forever.
 *
 * Since none of the OS/161 system calls have more than 4 arguments,
 * there should be no need to fetch additional arguments from the
 * user-level stack.
 *
 * Watch out: if you make system calls that have 64-bit quantities as
 * arguments, they will get passed in pairs of registers, and not
 * necessarily in the way you expect. We recommend you don't do it.
 * (In fact, we recommend you don't use 64-bit quantities at all. See
 * arch/mips/include/types.h.)
 */

void
mips_syscall(struct trapframe *tf) {
    int callno;
    int32_t retval;
    int err;

    assert(curspl == 0);

    callno = tf->tf_v0;

    /*
     * Initialize retval to 0. Many of the system calls don't
     * really return a value, just 0 for success and -1 on
     * error. Since retval is the value returned on success,
     * initialize it to 0 by default; thus it's not necessary to
     * deal with it except for calls that return other values, 
     * like write.
     */

    retval = 0;

    switch (callno) {
        case SYS_reboot:
            err = sys_reboot(tf->tf_a0);
            break;

        case SYS_fork:
            err = fork(tf, &retval);
            break;

        case SYS_getpid:
            err = getpid(&retval);
            break;

        case SYS_waitpid:
            err = waitpid(tf->tf_a0, (int*) tf->tf_a1, tf->tf_a2, &retval);
            break;

        case SYS__exit:
            _exit(tf->tf_a0);
            break;

        case SYS_read:
            err = read(tf->tf_a0, tf->tf_a1, tf->tf_a2, &retval, tf);
            break;

        case SYS_write:
            err = write(tf->tf_a0, tf->tf_a1, tf->tf_a2, &retval);
            break;

        case SYS_execv:
            err = execv((const char *) tf->tf_a0, (char *const *) tf->tf_a1, &retval);
            break;

        case SYS___time:
            err = __time(tf->tf_a0, tf->tf_a1, &retval, tf);
            break;

        case SYS_sbrk:
            err = sbrk(tf->tf_a0, &retval);
            break;

            /* Add stuff here ************************************************************************************/

        default:
            kprintf("Unknown syscall %d\n", callno);
            err = ENOSYS;
            break;
    }


    if (err) {
        /*
         * Return the error code. This gets converted at
         * userlevel to a return value of -1 and the error
         * code in errno.
         */
        tf->tf_v0 = err;
        tf->tf_a3 = 1; /* signal an error */
    } else {
        /* Success. */
        tf->tf_v0 = retval;
        tf->tf_a3 = 0; /* signal no error */
    }

    /*
     * Now, advance the program counter, to avoid restarting
     * the syscall over and over again.
     */

    tf->tf_epc += 4;

    /* Make sure the syscall code didn't forget to lower spl */
    assert(curspl == 0);
}

void md_forkentry(void* tf, unsigned long vmspace) {

    struct trapframe tfchild = *(struct trapframe*) tf;
    tfchild.tf_epc += 4;
    tfchild.tf_v0 = 0;
    tfchild.tf_a3 = 0;
    curthread->t_vmspace = (struct addrspace*) vmspace;
    as_activate((struct addrspace*) vmspace);
    mips_usermode(&tfchild);
    kfree(tf);
    return 0;
}

int fork(struct trapframe *tf, int* retval) {
    int spl = splhigh();
    int er;
    struct thread* childthread;



    //copy the trapframe
    struct trapframe* tf_child = kmalloc(sizeof (struct trapframe));

    if (tf_child == NULL) {
        splx(spl);
        return ENOMEM;
    }

    memcpy(tf_child, tf, sizeof (struct trapframe));

    //copy the address space
    struct addrspace* vmspace_child;
    er = as_copy(curthread->t_vmspace, &vmspace_child);
    if (vmspace_child == NULL) {
        splx(spl);
        return -1;
    }

    if (er) {
        splx(spl);
        return er;
    }

    int result = thread_fork(curthread->t_name, (void*) tf_child, (unsigned long) vmspace_child, md_forkentry, &childthread);

    if (result != 0) {

        splx(spl);
        return result;

    }

    *retval = childthread->processid;

    splx(spl);
    return 0;
}

int getpid(int* retpid) {

    *retpid = curthread->processid;

    return 0;

}

int waitpid(pid_t pid, int* status, int options, int *ret) {


    //check validity of pid
    if (pid > 255 || pid <= 0 || options != 0) {
        *ret = -1;
        return EINVAL;
    }
    //status is not valid
    if (status == NULL || status == (int *) 0x80000000 || status == (int *) 0x40000000) {
        *ret = -1;
        return EFAULT;
    }
    //did not find the process with this pid
    if (processTable[pid] == NULL) {
        *ret = -1;
        return EINVAL;
    }
    //check if the pid is the child of curthread
    if (processTable[pid]->parentPid != curthread->processid) {
        *ret = -1;
        return EINVAL;
    }
    //the current thread is waiting for itself
    if (pid == curthread->processid) {
        *ret = -1;
        return EINVAL;
    }

    //check if the aimed process has exited
    P(processTable[pid]->exit_wait_lock);

    copyout((const void*) &processTable[pid]->exit_code, status, sizeof (int));

    freeprocess(pid);
    *ret = pid;

    return 0;

}

void _exit(int exitcode) {

    processTable[curthread->processid]->exit_code = exitcode;

    V(processTable[curthread->processid]->exit_wait_lock);

    thread_exit();

    return;

}

int read(int fd, void* user_buf, size_t read_count, int* retval, struct trapframe *tf) {

    //int fd = tf->tf_a0;
    //char* user_buf = (char*)tf->tf_a1;
    //int read_count = tf->tf_a2;

    //EBADF		fd is not a valid file descriptor, or was not opened for reading.
    if (user_buf == NULL || user_buf == 0x40000000 || user_buf == 0x80000000) {
        *retval = -1;
        return EFAULT;
    }
    if (fd != 0) {
        *retval = -1;
        return EBADF;
    }

    int i;

    for (i = 0; i < read_count; i++) {
        char kbuf = getch();
        int result = copyout(&kbuf, (userptr_t) user_buf + i, 1);
        if (result != 0) {
            *retval = -1;
            return EFAULT;
        }
    }
    *retval = i;

    return 0;
}

int write(int fd, char* c, size_t size, int* retval) {

    int spl = splhigh();

    if (fd == 0 || fd > 2) {
        splx(spl);
        return EBADF;
    }
    if (c == 0x40000000 || c == 0x80000000) {
        splx(spl);
        return EFAULT;
    }
    char * kernel_buf = kmalloc(sizeof (char) * (size + 1));
    int res = copyin((const_userptr_t) c, kernel_buf, size);
    if (res) {
        splx(spl);
        return EFAULT;
    }
    kernel_buf[size] = '\0';
    int i;
    for (i = 0; i < size; i++) {
        putch(kernel_buf[i]);
    }
    *retval = size;
    kfree(kernel_buf);
    splx(spl);
    return 0;

}

int execv(const char *prog, char *const *args, int* retval) {

    if (prog == NULL || args == NULL || (int*) prog == 0x80000000 || (int*) prog == 0x40000000) {
        *retval = -1;
        return EFAULT;
    }
    if ((int*) args == 0x80000000 || (int*) args == 0x40000000) {
        *retval = -1;
        return EFAULT;
    }

    if (prog == '\0' || strcmp(prog, "") == 0) {
        *retval = -1;
        return EINVAL;
    }
    char* pathname = kmalloc(128 * sizeof (char));
    if (pathname == NULL) {
        *retval = -1;
        return ENOMEM;
    }

    //////////////////////////copy path name from user space to kernel space///////////////////////////////
    int result = copyinstr((const_userptr_t) prog, pathname, 128, NULL);

    if (result) {
        kfree(pathname);
        *retval = result;
        return EFAULT;
    }

    //get arg number and make a copy from args to argv
    int argc = 0;
    while (args[argc] != NULL) {
        argc = argc + 1;
    }

    char** argv;
    argv = kmalloc(sizeof (userptr_t)*(argc + 1));
    if (argv == NULL) {
        return ENOMEM;
    }

    //copy the address of each string
    result = copyin((const_userptr_t) args, argv, sizeof (userptr_t)*(argc + 1));

    if (result) {
        kfree(pathname);
        kfree(argv);
        *retval = result;
        return EFAULT;
    }

    //copy strings from args to argv
    int i;
    int stringlength;
    for (i = 0; i < argc; i++) {
        //it is just hardcoding........
        if (args[i] == 0x80000000 || args[i] == 0x40000000) {
            kfree(argv);
            kfree(pathname);
            *retval = -1;
            return EFAULT;
        }

        argv[i] = kmalloc(sizeof (char) * (strlen(args[i]) + 1));
        //no available memory space, free everything	
        if (argv == NULL) {
            int k;
            for (k = 0; k < i; k++) {
                kfree(argv[k]);
            }
            kfree(argv);
            kfree(pathname);
            return ENOMEM;
        }

        //copy
        result = copyinstr((const_userptr_t) args[i], argv[i], strlen(args[i]) + 1, NULL);

        if (result) {
            int k;
            for (k = 0; k < i; k++) {
                kfree(argv[k]);
            }
            kfree(argv);
            kfree(pathname);
            *retval = result;
            return EFAULT;
        }
    }


    ////////////////////////////////////////////////open the file//////////////////////////////////////////
    struct vnode* vn; //current working directory type
    vaddr_t entrypoint, stackpointer;

    result = vfs_open(pathname, 0, &vn);

    if (result) {
        for (i = 0; i < argc; i++) {
            kfree(argv[i]);
        }
        kfree(argv);
        kfree(pathname);
        return result;
    }

    //////////////////////////////////////deal with virtual memory//////////////////////////////////////////
    //erase current virtual memory space
    if (curthread->t_vmspace != NULL) {
        as_destroy(curthread->t_vmspace);
        curthread->t_vmspace = NULL;
    }

    //create a new virtual memory space
    curthread->t_vmspace = as_create();
    //no memory
    if (curthread->t_vmspace == NULL) {
        vfs_close(vn);
        for (i = 0; i < argc; i++) {
            kfree(argv[i]);
        }
        kfree(argv);
        kfree(pathname);
        return ENOMEM;
    }

    as_activate(curthread->t_vmspace);
    result = load_elf(vn, &entrypoint);
    if (result) {
        vfs_close(vn);
        for (i = 0; i < argc; i++) {
            kfree(argv[i]);
        }
        kfree(argv);
        kfree(pathname);
        return result;
    }


    //done with the file
    vfs_close(vn);

    //////////////////////////////////////////deal with stack///////////////////////////////////////////////
    result = as_define_stack(curthread->t_vmspace, &stackpointer);

    if (result) {
        for (i = 0; i < argc; i++) {
            kfree(argv[i]);
        }
        kfree(argv);
        kfree(pathname);
        return result;
    }

    for (i = argc - 1; i >= 0; i--) {

        if ((strlen(argv[i]) + 1) <= 4) {
            stringlength = 1;
        }
        else if ((strlen(argv[i]) + 1) % 4 == 0) {
            stringlength = (strlen(argv[i]) + 1) / 4;
        }
        else if ((strlen(argv[i]) + 1) % 4 != 0) {
            stringlength = (strlen(argv[i]) + 1) / 4 + 1;
        }

        stackpointer = stackpointer - stringlength * 4;

        result = copyoutstr(argv[i], (userptr_t) stackpointer, strlen(argv[i]) + 1, NULL);

        if (result) {
            int j;
            for (j = 0; j < argc; j++) {
                kfree(argv[j]);
            }
            kfree(argv);
            kfree(pathname);
            return result;
        }

        kfree(argv[i]);

        argv[i] = (char*) stackpointer;

    }

    int* null = NULL;
    stackpointer = stackpointer - 4;
    null = (int*) stackpointer;
    *null = 0x00000000;

    for (i = argc - 1; i >= 0; i--) {

        stackpointer = stackpointer - 4;
        result = copyout(&argv[i], (userptr_t) stackpointer, sizeof (vaddr_t));

        if (result) {
            kfree(argv);
            kfree(pathname);
            return result;
        }
    }

    kfree(argv);
    kfree(pathname);

    md_usermode(argc, (userptr_t) stackpointer, stackpointer, entrypoint);

    panic("md_usermode returned\n");
    return EINVAL;

}


int __time(time_t *seconds, unsigned long *nanoseconds, int* retval, struct trapframe tf){
    if (seconds == NULL && nanoseconds == NULL){
        return EINVAL;
    }
    if (seconds == 0x40000000 || seconds == 0x80000000 || nanoseconds == 0x80000000 || nanoseconds == 0x40000000){
        return EFAULT;
    }
    if (seconds == NULL){
//        unsigned long kernel_nano;
        time_t get_seconds;
        gettime(&get_seconds, (u_int32_t)nanoseconds);
//        kprintf("\nnano with no seconds: %d\n", (int)*get_seconds);
        *retval = (int)get_seconds;
        return 0;
    }
    else if (nanoseconds == NULL){
        u_int32_t get_nano;
        gettime(seconds, &get_nano);
//        kprintf("\nsecond with no nanos: %d\n", (int)*seconds);
        *retval = (int)*seconds;
        return 0;
    }
    else {
        gettime(seconds, (u_int32_t)nanoseconds);
//        kprintf("\nnano: %d,,,seconds: %d\n", (int)*seconds, (int)*nanoseconds);
        *retval = (int)*seconds;
        return 0;
    }
}

int sbrk(int amount, int32_t* retval) {
	struct addrspace* as = curthread->t_vmspace;
	if (as->as_heap_end + amount < as->as_heap_start) {
		return EINVAL;
	}
	if (as->as_heap_end + amount >= USERSTACK - 24 * 4096){
		return ENOMEM;
	}
        if (as->as_heap_end < 0){
            return EINVAL;
        }
	*retval = as->as_heap_end;
	as->as_heap_end += amount;
	return 0;
}














































