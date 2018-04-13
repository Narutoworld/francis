#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/spl.h>
#include <machine/tlb.h>
#include <thread.h>
#include <curthread.h>

#include "elf.h"
//#include <string.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */
#define DUMBVM_STACKPAGES    512


extern int first_coremap_page_num;
extern int max_coremap_entries;
extern int num_coremap_free;

vaddr_t
alloc_kpages(int npages) {
    int s;
    s = splhigh();
    paddr_t pa;
    pa = getppages(npages);
    if (pa == 0) {
        splx(s);
        return 0;
    }
    splx(s);
    return PADDR_TO_KVADDR(pa);
}

void
free_kpages(vaddr_t addr) {
    int s;
    s = splhigh();
    if (curthread->t_vmspace == 0x0) {
        splx(s);
        return;
    }
    int haha;
    if (addr & 0x80000000) {
        int i;
        for (i = 0; i < coremapTable[KVADDR_TO_PAGE_FRAME(addr)].length; i++) {
            coremapTable[KVADDR_TO_PAGE_FRAME(addr) + i].status = 'H';
//            int q;
//            char * phy_ptr = (char *) PADDR_TO_KVADDR(coremapTable[KVADDR_TO_PAGE_FRAME(addr) + i].selfAddr);
//            for (q = 0; q < 4096; q++) {
//                phy_ptr[q] = 0;
//            }
        }
        //        coremap_print();
        splx(s);
        return;
    }
    int i;
    unsigned int first_level_entry_num = addr & 0x7fc00000;
    first_level_entry_num = first_level_entry_num >> 22;
    unsigned int second_level_page_entry_num = addr & 0x003ff000;
    second_level_page_entry_num = second_level_page_entry_num >> 12;
    struct page_table_entry* page_table;
    //    coremap_print();
    //    kprintf("address space : %x, curthread : %x\n", curthread->t_vmspace, curthread);
    page_table = curthread->t_vmspace->level_one_page_table[first_level_entry_num];
    //    kprintf("free address: %x\n", addr);
    //    for(i = 0; i < 1024; i++){
    //        kprintf("i: %d; page_table address: %x\n", i, curthread->t_vmspace->level_one_page_table[i]);
    //    }
    //        kprintf("page_table address: %x\n", page_table);
    int page_frame_num_start = KVADDR_TO_PAGE_FRAME(page_table[second_level_page_entry_num].page_frame_address);
    int page_length = coremapTable[page_frame_num_start].length;

    /*if associated page is only itself*/
    if (page_length == 1) {
        assert(coremapTable[page_frame_num_start].status == 'P');
        coremapTable[page_frame_num_start].status = 'H';
        coremapTable[page_frame_num_start].as = NULL;
        page_table[second_level_page_entry_num].page_frame_address = 0;
        num_coremap_free++;
        //            curthread->t_vmspace->level_two_page_table_counter[first_level_entry_num]++;
    } else {

        /*if more than one page is associated with the address*/
        while (page_length >= 1) {
            assert(coremapTable[page_frame_num_start].status == 'P');
            coremapTable[page_frame_num_start].status = 'H';
            coremapTable[page_frame_num_start].as = NULL;
            int first_entry;
            int second_entry;

            /*get two level entries*/
            first_entry = coremapTable[page_frame_num_start].first_level_entry;
            second_entry = coremapTable[page_frame_num_start].second_level_entry;
            struct page_table_entry* second_level_table;
            second_level_table = curthread->t_vmspace->level_one_page_table[first_entry];
            assert(KVADDR_TO_PAGE_FRAME(second_level_table[second_entry].page_frame_address) == page_frame_num_start);

            /*reset information in second level table*/
            second_level_table[second_entry].page_frame_address = 0;
            page_frame_num_start++;
            page_length--;
            //                curthread->t_vmspace->level_two_page_table_counter[first_entry] += 1;
        }
    }
    //coremap_print();
    /* nothing */
    splx(s);
    (void) addr;
}

static
paddr_t
getppages(unsigned long npages) {
    int spl;
    paddr_t addr;

    spl = splhigh();
    int i;
    int found = 0;

    /*check continuous holes*/
    for (i = 0; i < max_coremap_entries; i++) {
        if (coremapTable[i].status == 'H') {
            int counter;
            for (counter = 0; i + counter < max_coremap_entries; counter++) {
                if (coremapTable[i + counter].status == 'P') {
                    break;
                }
                if (counter == npages) {
                    found = 1;
                    break;
                }
            }
            if (found) {
                //                kprintf("start index: %d  page length: %d\n", i, npages);
                break;
            }
        }
    }
    //    
    if (found == 1) {
        int k;
        for (k = 0; k < npages; k++) {
            coremapTable[i + k].status = 'P';
            if (curthread != 0x0) {
                coremapTable[i + k].as = curthread->t_vmspace;
            }
            coremapTable[i + k].length = npages;
            coremapTable[i + k].start = (k + first_coremap_page_num) * PAGE_SIZE;
//            int q;
//            char * phy_ptr = (char *) PADDR_TO_KVADDR(coremapTable[i + k].selfAddr);
//            for (q = 0; q < 4096; q++) {
//                phy_ptr[q] = 0;
//            }
        }
    }

    addr = (paddr_t) (i + first_coremap_page_num) * PAGE_SIZE;
    num_coremap_free = num_coremap_free - npages;

    splx(spl);




    return addr;
}

struct addrspace *
as_create(void) {
    struct addrspace *as = kmalloc(sizeof (struct addrspace));
    if (as == NULL) {
        return NULL;
    }
    //    kprintf("\n-------address space : %x curthread : %x--------\n", as, curthread);

    /*
     * Initialize as needed.
     */

    as->as_vbase1 = 0;
    as->as_npages1 = 0;
    as->as_vbase2 = 0;
    as->as_npages2 = 0;
    as->as_heap_start = 0;
    as->as_heap_end = 0;
    as->as_stackpbase = 0;
    int i;
    for (i = 0; i < 1024; i++) {
        as->level_one_page_table[i] = NULL;
        //        as->level_two_page_table_counter[i] = 0;
    }
    return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret) {
    int spl;
    spl = splhigh();
    /*
     * Write this.
     */

    struct addrspace *new;
    //
    new = as_create();
    if (new == NULL) {
        splx(spl);
        return ENOMEM;
    }

//    new->as_vbase1 = old->as_vbase1;
//    new->as_npages1 = old->as_npages1;
//    new->as_vbase2 = old->as_vbase2;
//    new->as_npages2 = old->as_npages2;
//    new->as_heap_end = old->as_heap_end;
//    new->as_heap_start = old->as_heap_start;
//
//    if (as_prepare_load(new)) {
//        as_destroy(new);
//        splx(spl);
//        return ENOMEM;
//    }
//
//    assert(new->as_stackpbase != 0);
    //    memmove((void *) PADDR_TO_KVADDR(new->as_stackpbase),
    //            (const void *) PADDR_TO_KVADDR(old->as_stackpbase),
    //            DUMBVM_STACKPAGES * PAGE_SIZE);
    /*malloc spaces for needed level two page table first*/
//    int i;
//    for (i = 0; i < 1024; i++) {
//        if (old->level_one_page_table[i] == NULL)
//            new->level_one_page_table[i] == NULL;
//        else {
//            new->level_one_page_table[i] = kmalloc(1024 * sizeof (struct page_table_entry));
//            if (new->level_one_page_table[i] == NULL)
//                return ENOMEM;
//            struct page_table_entry* new_second_level_page_table = new->level_one_page_table[i];
//            int j;
//        }
//    }
//
//    for (i = 0; i < 1024; i++) {
//        //        new->level_two_page_table_counter[i] = old->level_two_page_table_counter[i];
//        struct page_table_entry* old_second_level_page_table = old->level_one_page_table[i];
//        struct page_table_entry* new_second_level_page_table = new->level_one_page_table[i];
//        if (old_second_level_page_table != 0 && new_second_level_page_table != 0) {
//            int j;
//            for (j = 0; j < 1024; j++) {
//
//                if (old_second_level_page_table[j].page_frame_address < 524288 && old_second_level_page_table[j].page_frame_address >= first_coremap_page_num * PAGE_SIZE) {
//                    /*if the second level entry is not copied yet*/
//                    /*alloc one page without considering swapping*/
//                    if (coremapTable[KVADDR_TO_PAGE_FRAME(old_second_level_page_table[j].page_frame_address)].length == 1) {
//                        unsigned int work_address = alloc_kpages(1);
//                        unsigned int source_address = old_second_level_page_table[j].page_frame_address;
//                        int work_coremap_index = (work_address - 0x80000000) / PAGE_SIZE - first_coremap_page_num;
//                        new_second_level_page_table[j].page_frame_address = source_address;
//                        coremapTable[work_coremap_index].first_level_entry = i;
//                        coremapTable[work_coremap_index].second_level_entry = j;
//                        coremapTable[work_coremap_index].as = new;
//                        int q;
//                        char * phy_ptr = (char *) work_address;
//                        for (q = 0; q < 4096; q++) {
//                            phy_ptr[q] = 0;
//                        }
//                        memmove(work_address, source_address, PAGE_SIZE);
//                    } else {
//                        /*if the length is more that 1, need to allocate continuous physical memory*/
//                        int work_length = coremapTable[KVADDR_TO_PAGE_FRAME(old_second_level_page_table[j].page_frame_address)].length;
//                        int old_coremap_start_index = KVADDR_TO_PAGE_FRAME(old_second_level_page_table[j].page_frame_address);
//                        unsigned int work_start_address = alloc_kpages(work_length);
//                        if (work_start_address == 0) {
//                            return ENOMEM;
//                        }
//                        int work_start_index = KVADDR_TO_PAGE_FRAME(work_start_address);
//                        int q;
//                        char * phy_ptr = (char *) work_start_address;
//                        for (q = 0; q < 4096 * work_length; q++) {
//                            phy_ptr[q] = 0;
//                        }
//                        int k;
//                        for (k = 0; k < work_length; k++) {
//                            coremapTable[work_start_index + k].as = new;
//                            coremapTable[work_start_index + k].first_level_entry = coremapTable[old_coremap_start_index + k].first_level_entry;
//                            coremapTable[work_start_index + k].second_level_entry = coremapTable[old_coremap_start_index + k].second_level_entry;
//                            coremapTable[work_start_index + k].length = coremapTable[old_coremap_start_index + k].length;
//                            coremapTable[work_start_index + k].startAddr = KVADDR_TO_PADDR(work_start_address);
//                            int l2_first_entry, l2_second_entry;
//                            l2_first_entry = coremapTable[work_start_index + k].first_level_entry;
//                            l2_second_entry = coremapTable[work_start_index + k].second_level_entry;
//                            new->level_one_page_table[l2_first_entry][l2_second_entry].page_frame_address = work_start_address + k * PAGE_SIZE;
//                        }
//                        memmove(work_start_address, old_second_level_page_table[j].page_frame_address, work_length * PAGE_SIZE);
//                    }
//
//                }
//            }
//        }
//    }
    *ret = new;
    splx(spl);
    return 0;
}

void
as_destroy(struct addrspace *as) {
    /*
     * Clean up as needed.
     */
    int spl;
    spl = splhigh();
    int i;
    for (i = 0; i < max_coremap_entries; i++) {
        if (coremapTable[i].as == as) {
            coremapTable[i].status = 'H';
            coremapTable[i].as = NULL;
            coremapTable[i].length = 0;
            coremapTable[i].start = 0;
            coremapTable[i].fixed = 0;
            coremapTable[i].referenced = 0;
            coremapTable[i].counter = 0;
            coremapTable[i].modified = 0;
            coremapTable[i].copy = 0;
            coremapTable[i].first_level_entry = -1;
            coremapTable[i].second_level_entry = -1;
        }
    }
    for (i = 0; i < 1024; i++) {
        if (as->level_one_page_table[i] != NULL) {
            kfree(as->level_one_page_table[i]);
        }
    }
    kfree(as->level_one_page_table);

    kfree(as);
    splx(spl);
}

void
as_activate(struct addrspace *as) {
    /*
     * Write this.
     */
    int i, spl;

    (void) as;

    spl = splhigh();

    for (i = 0; i < NUM_TLB; i++) {
        TLB_Write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
    }

    splx(spl);
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
        int readable, int writeable, int executable) {
    /*
     * Write this.
     */
    size_t npages;

    /* Align the region. First, the base... */
    sz += vaddr & ~(vaddr_t) PAGE_FRAME;
    vaddr &= PAGE_FRAME;

    /* ...and now the length. */
    sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

    npages = sz / PAGE_SIZE;

    /* We don't use these - all pages are read-write */
    (void) readable;
    (void) writeable;
    (void) executable;

    if (as->as_vbase1 == 0) {
        as->as_vbase1 = vaddr;
        as->as_npages1 = npages;
        return 0;
    }

    if (as->as_vbase2 == 0) {
        as->as_vbase2 = vaddr;
        as->as_heap_start = vaddr + npages * PAGE_SIZE;
        as->as_heap_end = as->as_heap_start;
        as->as_npages2 = npages;
        return 0;
    }

    /*
     * Support for more than two regions is not available.
     */
    kprintf("dumbvm: Warning: too many regions\n");

    (void) as;
    (void) vaddr;
    (void) sz;
    (void) readable;
    (void) writeable;
    (void) executable;
    return EUNIMP;
}

int
as_prepare_load(struct addrspace *as) {
    /*
     * Write this.
     */
    assert(as->as_stackpbase == 0);

    as->as_stackpbase = alloc_kpages(DUMBVM_STACKPAGES);
    if (as->as_stackpbase == 0) {
        return ENOMEM;
    }
    return 0;
}

int
as_complete_load(struct addrspace *as) {
    /*
     * Write this.
     */

    (void) as;
    return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr) {
    /*
     * Write this.
     */
    assert(as->as_stackpbase != 0);
    (void) as;

    /* Initial user-level stack pointer */
    *stackptr = USERSTACK;

    return 0;
}

