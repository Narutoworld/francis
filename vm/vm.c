/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <curthread.h>
#include <addrspace.h>
#include <vm.h>
#include <machine/spl.h>
#include <machine/tlb.h>


extern int first_coremap_page_num;
extern int max_coremap_entries;
extern int num_coremap_free;



#define DUMBVM_STACKPAGES    512

void
vm_bootstrap(void) {
    /* Do nothing. */
}

int
vm_fault(int faulttype, vaddr_t faultaddress) {
    vaddr_t vbase1, vtop1, vbase2, vtop2, vbase3, vtop3, stackbase, stacktop;
    paddr_t paddr;
    int i;
    u_int32_t ehi, elo;
    struct addrspace *as;
    int spl;
    int fault_value;
    unsigned int offset;
    offset = faultaddress & 0x00000fff;
    /*fault value is 0 on READ, 1 on WRITE*/
    int kernel;
    if (faultaddress & 0x80000000 == 0x80000000)
        kernel = 1;
    else kernel = 0;

    spl = splhigh();

    faultaddress &= PAGE_FRAME;

    DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);
//    kprintf("VM_FAULT: %x\n", faultaddress);
    
    switch (faulttype) {
        case VM_FAULT_READONLY:
            /* We always create pages read-write, so we can't get this */
            panic("dumbvm: got VM_FAULT_READONLY\n");
            break;
        case VM_FAULT_READ:
            fault_value = 0;
            break;
        case VM_FAULT_WRITE:
            fault_value = 1;
            break;
        default:
            splx(spl);
            return EINVAL;
    }

    as = curthread->t_vmspace;
    if (as == NULL) {
        /*
         * No address space set up. This is probably a kernel
         * fault early in boot. Return EFAULT so as to panic
         * instead of getting into an infinite faulting loop.
         */
        return EFAULT;
    }

    /* Assert that the address space has been set up properly. */
    assert(as->as_vbase1 != 0);
    assert(as->as_npages1 != 0);
    assert(as->as_vbase2 != 0);
    assert(as->as_npages2 != 0);
    assert(as->as_stackpbase != 0);
    assert((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
    assert((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);
    assert((as->as_stackpbase & PAGE_FRAME) == as->as_stackpbase);

    vbase1 = as->as_vbase1;
    vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
    vbase2 = as->as_vbase2;
    vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
    vbase3 = as->as_heap_start;
    vtop3 = as->as_heap_end;
    stackbase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
    stacktop = USERSTACK;
    int under_stack_address = 0;

    if (faultaddress >= vbase1 && faultaddress < vtop1) {
        paddr = faultaddress - vbase1;
    } else if (faultaddress >= vbase2 && faultaddress < vtop2) {
        paddr = faultaddress - vbase2;
    } else if (faultaddress >= vbase3 && faultaddress < vtop3) {
        paddr = faultaddress - vbase3;
    } else if (faultaddress >= stackbase && faultaddress < stacktop) {
        under_stack_address = 1;
        paddr = (faultaddress - stackbase) + as->as_stackpbase;
    } else {
        splx(spl);
        return EFAULT;
    }

    /*level one entry index and level two entry index*/
    unsigned int level_one_entry_index;
    level_one_entry_index = faultaddress & 0x7fc00000;
    level_one_entry_index = level_one_entry_index >> 22;
    unsigned int level_two_entry_index;
    level_two_entry_index = faultaddress & 0x003ff000;
    level_two_entry_index = level_two_entry_index >> 12;
    struct page_table_entry* level_two_page_table;
    int page_frame_index;
    
    /*if not fault on kernel*/
    if (!kernel) {
        if (curthread->t_vmspace->level_one_page_table[level_one_entry_index] == NULL) {
            curthread->t_vmspace->level_one_page_table[level_one_entry_index] = kmalloc(1024 * sizeof (struct page_table_entry));

            /*kmalloc address for the second level page table*/
            if (curthread->t_vmspace->level_one_page_table[level_one_entry_index] == NULL) {
                splx(spl);
                return ENOMEM;
            }
            level_two_page_table = curthread->t_vmspace->level_one_page_table[level_one_entry_index];
            int i;

            /*initialize level two page entries*/
            for (i = 0; i < 1024; i++) {
                level_two_page_table[i].page_frame_address = 0;
            }
        }
        level_two_page_table = curthread->t_vmspace->level_one_page_table[level_one_entry_index];
        
        
        /*if no page frame is associated with this level two entry*/
        if (!(KVADDR_TO_PADDR(level_two_page_table[level_two_entry_index].page_frame_address) < 524288 && KVADDR_TO_PADDR(level_two_page_table[level_two_entry_index].page_frame_address) > first_coremap_page_num * PAGE_SIZE)) {
            unsigned int data_address;
            data_address = alloc_kpages(1);
            if (data_address == 0) {
                splx(spl);
                return ENOMEM;
            }
            if (!(KVADDR_TO_PADDR(data_address) < 524288 && KVADDR_TO_PADDR(data_address) > first_coremap_page_num * PAGE_SIZE)){
                splx(spl);
                return ENOMEM;
            }
            level_two_page_table[level_two_entry_index].page_frame_address = data_address;
            page_frame_index = KVADDR_TO_PAGE_FRAME(level_two_page_table[level_two_entry_index].page_frame_address);
            coremapTable[page_frame_index].first_level_entry = level_one_entry_index;
            coremapTable[page_frame_index].second_level_entry = level_two_entry_index;
        }

        /*level two entry exists but not loaded on TLB*/
//        coremapTable[page_frame_index].referenced = 1;
//        if (fault_value == 1) {
//            /*fault on WRITE*/
//            coremapTable[page_frame_index].modified = 1;
//        }
        for (i = 0; i < NUM_TLB; i++) {
            TLB_Read(&ehi, &elo, i);
            if (elo & TLBLO_VALID) {
                continue;
            }
            ehi = faultaddress;
            elo = KVADDR_TO_PADDR(level_two_page_table[level_two_entry_index].page_frame_address) | TLBLO_DIRTY | TLBLO_VALID;
//            kprintf("dumbvm: 0x%x -> 0x%x\n", ehi, elo);
            DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
            TLB_Write(ehi, elo, i);
            splx(spl);
            return 0;
        }
        ehi = faultaddress;
        elo = KVADDR_TO_PADDR(level_two_page_table[level_two_entry_index].page_frame_address) | TLBLO_DIRTY | TLBLO_VALID;
//        kprintf("dumbvm: 0x%x -> 0x%x\n", ehi, elo);
        TLB_Random(ehi, elo);
        splx(spl);
        return 0;
    }
    else {
        /*if fault on kernel*/
        for (i = 0; i < NUM_TLB; i++) {
            TLB_Read(&ehi, &elo, i);
            if (elo & TLBLO_VALID) {
                continue;
            }
            ehi = faultaddress;
            elo = KVADDR_TO_PADDR(faultaddress) | TLBLO_DIRTY | TLBLO_VALID;
            DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
            TLB_Write(ehi, elo, i);
            splx(spl);
            return 0;
        }
        ehi = faultaddress;
        elo = KVADDR_TO_PADDR(faultaddress) | TLBLO_DIRTY | TLBLO_VALID;
        TLB_Random(ehi, elo);

        splx(spl);
        return 0;
    }



    kprintf("dumbvm: Ran out of TLB entries - cannot handle page fault\n");
    splx(spl);
    return EFAULT;
}
