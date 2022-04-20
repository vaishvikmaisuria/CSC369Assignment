#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"


extern int memsize;

extern int debug;

extern struct frame *coremap;

/* Page to evict is chosen using the clock algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int start, circular_hand;
int clock_evict() {
	start = circular_hand;
	
	while(start < memsize +1){
		

		pgtbl_entry_t *page_table = coremap[start].pte;
	
		//implies that page was recently used thus not good for replacement 
		if((page_table->frame & PG_REF) == PG_REF){
			// set the PG_REF bit to 0 
			page_table->frame = page_table->frame & ~PG_REF;

		}else{
			// set the PG_REF bit to 1 
			//page_table->frame = page_table->frame | PG_REF;
			circular_hand = start;
			return circular_hand;
		}
		start = (start+1)%memsize;
	}

	return 0;
	
}

/* This function is called on each access to a page to update any information
 * needed by the clock algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void clock_ref(pgtbl_entry_t *p) {
	
	return;
}

/* Initialize any data structures needed for this replacement
 * algorithm. 
 */
void clock_init() {
	start = 0;
	circular_hand = 0;
}
