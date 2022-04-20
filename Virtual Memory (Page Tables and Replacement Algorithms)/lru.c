#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"

int in_list(int frame, Qnode *element);
void printlist(Qnode *element);

extern int memsize;

extern int debug;

extern struct frame *coremap;

Qnode *first_element;
Qnode *last_element;

/* Page to evict is chosen using the accurate LRU algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */

int lru_evict() {
	//take the frame at the head since it was used last
	int frame = first_element->frame;
	//make the second element, the first element
	first_element = first_element->next;
	//free the first element and point the new first element to NULL
	free(first_element->prev);
	first_element->prev = NULL;
	return frame;

}

/* This function is called on each access to a page to update any information
 * needed by the lru algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void lru_ref(pgtbl_entry_t *p) {
	int cur_frame = p->frame >> PAGE_SHIFT;
	if(first_element == NULL){
		//zero elements in the list
		//create a new element
		Qnode *new_element = malloc(sizeof(struct Qnode));
		//initialize
		new_element->frame = cur_frame;
		new_element->next=NULL;
		new_element->prev=NULL;
		//point the first and last pointer at it
		first_element = new_element;
		last_element = first_element;
	}
	else if(first_element == last_element){
		//one element in the list
		if(in_list(cur_frame,first_element) == 0){ //if an existing element was referenced
			//create a new element
			Qnode *new_element = malloc(sizeof(struct Qnode));
			//initialize
			new_element->frame = cur_frame;
			//link first to the element
			first_element->next = new_element;
			//attach the element to the end of the list
			last_element = new_element;
			new_element->next = NULL; 
			new_element->prev = first_element;
		}
	}
	else{
		//many elements in the list
		if(in_list(cur_frame,first_element) == 0){ //if an existing element was referenced
			//create a new element
			Qnode *new_element = malloc(sizeof(struct Qnode));
			//attach the element to the end of the list
			//initialize
			new_element->frame = cur_frame;
			new_element->next = NULL;
			//attach element to the end of the list
			new_element->prev = last_element;
			last_element->next = new_element;
			last_element = new_element;
		}
	}
	return;
}

void printlist(Qnode *element){
	//print and element while there are elements left
	while(element != NULL){
		element = element->next;
	}
}

int in_list(int frame, Qnode *element){
	while(element != NULL){
		//if the frame already exists in an element
		if(element->frame == frame){
			//if it's the first element
			if(element->prev == NULL){
				//link backwards to null
				element->next->prev = NULL;
				first_element = element->next;
			}
			//not first
			else{
				//link neighbors backwards
				element->prev->next=element->next;
			}
			//if element is last
			if(element->next==NULL){
				//link forward to null
				element->prev->next = NULL;
				last_element = element->prev;
				
			}
			//not last
			else{
				//link neighbors forwards
				element->next->prev=element->prev;
			}
			//rip off element from the list
			element->next = NULL;
			element->prev = NULL;
			//add it to the end of the list
			last_element->next = element;
			element->prev = last_element;
			last_element = element;
			return 1;
		}
		element = element->next;
	}
	return 0;
}


/* Initialize any data structures needed for this 
 * replacement algorithm 
 */
void lru_init() {
	first_element = NULL;
	last_element = NULL;
}
