/* Author(s): <Your name here>
 * Implementation of the memory manager for the kernel.
 */

#include "common.h"
#include "interrupt.h"
#include "kernel.h"
#include "memory.h"
#include "printf.h"
#include "scheduler.h"
#include "util.h"

#define MEM_START 0xa0908000
 node_t	busy_page;
 node_t	spare_page;
/* Static global variables */
// Keep track of all pages: their vaddr, status, and other properties
static page_map_entry_t page_map[ PAGEABLE_PAGES ];

// other global variables...
/*static lock_t page_fault_lock;*/

/* TODO: Returns virtual address of page number i */
uint32_t page_vaddr( int i ) {
    return page_map[i].vaddr;
}

/* TODO: Returns  physical address (in kernel) of page number i */
uint32_t page_paddr( int i ) {
    return page_map[i].paddr;
}

/* get the physical address from virtual address (in kernel) */
uint32_t va2pa( uint32_t va ) {
    return (uint32_t) va - 0xa0000000;
}

/* get the virtual address (in kernel) from physical address */
uint32_t pa2va( uint32_t pa ) {
    return (uint32_t) pa + 0xa0000000;
}


// TODO: insert page table entry to page table
void insert_page_table_entry( uint32_t *table, uint32_t vaddr, uint32_t paddr,
                              uint32_t flag, uint32_t pid ) {
    // insert entry
	PTE* T = (PTE*)table;
	uint32_t	entrylo;
	while(1){
		if((T->entryhi)>>13 == vaddr){
			entrylo = (paddr >> 12) << 6;
			if(flag == 0){
				T->entrylo_0 |=  entrylo;
			}
			else if(flag == 1){
				T->entrylo_1 |=  entrylo;
			}
			break;
		}
		else	T++;
	}
	tlb_flush(T->entryhi);
    // tlb flush
}

/* TODO: Allocate a page. Return page index in the page_map directory.
 *
 * Marks page as pinned if pinned == TRUE.
 * Swap out a page if no space is available.
 */
int page_alloc( int pinned ) {
 
    // code here
	page_map_entry_t *chose;
	int	i;
	for(i = 0; i < PAGEABLE_PAGES; i++){
		if(page_map[i].used == 0){
			page_map[i].used = 1;
			page_map[i].pin = pinned;
			return i;
		}
	}
}

/* TODO: 
 * This method is only called once by _start() in kernel.c
 */
uint32_t init_memory( void ) {
    // initialize all pageable pages to a default state
	uint32_t begin = MEM_START;
	queue_init( &busy_page );
	queue_init( &spare_page );
	int	i;
	for(i = 0;i < PAGEABLE_PAGES;i++,begin+=PAGE_SIZE){
		page_map[i].num = i;
		page_map[i].vaddr = begin;
		page_map[i].paddr = va2pa(begin);
		page_map[i].used = 0;
		page_map[i].pin = 0;
		//enqueue(&spare_page,(node_t*)(&page_map[i]));
	}
	return 0;
}

void create_pte(uint32_t vaddr, int pid,PTE* pte,uint32_t daddr) {
	pte->entryhi = (vaddr<<13)|pid;
	pte->entrylo_0 = PE_G | PE_V | PE_D | PE_UC;
	pte->entrylo_1 = PE_G | PE_V | PE_D | PE_UC;
	pte->daddr = daddr;
    return;
}
/* TODO: 
 * 
 */
uint32_t setup_page_table( int pid ) {
	if(pid == 0)	return 0;
    uint32_t page_table;
	int	num = pcb[pid].size / PAGE_SIZE;
	int vaddr = pcb[pid].entry_point;
	int	i;
	int	loc = pcb[pid].loc;
	PTE* pte;
	int	page_num1,page_num2;
	uint32_t *test;
    // alloc page for page table 
	page_table = page_alloc(1);
	pte = (PTE*)page_vaddr(page_table);
    // initialize PTE and insert several entries into page tables using insert_page_table_entry
    for(i=0; i<=num; i+=2)
    {
		create_pte(vaddr>>13,pid,pte,loc);
		page_num1 = page_alloc( 0);
		bcopy((char*)loc,(char*)page_vaddr(page_num1),PAGE_SIZE);
		insert_page_table_entry(page_vaddr(page_table),vaddr>>13,page_paddr(page_num1),0,pid);
		loc += PAGE_SIZE;
		page_num2 = page_alloc( 0);
		bcopy((char*)loc,(char*)page_vaddr(page_num2),PAGE_SIZE);
		insert_page_table_entry(page_vaddr(page_table),vaddr>>13,page_paddr(page_num2),1,pid);
		pte++;
		vaddr+=(PAGE_SIZE<<1);
		loc+=PAGE_SIZE;
	}
    return page_table;
}

uint32_t do_tlb_miss(uint32_t vaddr, int pid) {
	enter_critical();
	PTE* T = (PTE*)page_vaddr(pcb[pid].page_table);
	while(1){
		if(((T->entryhi)>>13) == vaddr && ((T->entryhi)&0x0fff) == pid){
			return (uint32_t)T;
		}
		else T++;
	}
}


