#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include "sim.h"
#include "pagetable.h"


extern int debug;

extern struct frame *coremap;

int Tcounter;


//special structs node linked list
typedef struct Bnode {
	//index value for that frame
	int index;
	int vaddr;
    struct Bnode *next;
} Bnode;

typedef struct table{
    int vaddr;
    struct Bnode *list;
	struct table *next;
}table;

struct table *t;

void cut_tail(struct table *t);
void print_future(struct table *t);

// function to insert inside the table 
void insert_list(struct table *t, int frame, int index){
	//create new index node
	struct Bnode *newNode = (struct Bnode *)malloc(sizeof(struct Bnode));
	newNode->index = index;
	newNode->next = NULL;
	
	int found = 0;
	//go through every element in table
	while(t->next != NULL){
		//if we find the same address, append it to the linked list inside this element
		if(t->vaddr == frame){
			struct Bnode *node = t->list;
			while(node->next!=NULL){
				node = node->next;
			}
			node->next = newNode;
			//we already added an element in this function
			found = 1;
		}
		t = t->next;
	}
	//if we havn't added the element yet
	if(!found){
		//add the element to the end of the table list
		t->vaddr = frame;
		t->list = newNode;
		t->next = malloc(sizeof(struct table));
	}
}

/* Page to evict is chosen using the optimal (aka MIN) algorithm. 
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int opt_evict() {
	int frame = 0;
	int max;
	struct table *temp = t;
	int i = 0;
	while(temp!=NULL){
		//if the table is null then there is no future, use this frame
		if(temp->list == NULL){
			frame = i;
			return frame;
			
		}
		//determine max wait
		else{
			if(temp->list->index > max){
				max = temp->list->index;
				//update frame
				frame = temp->vaddr;
			}
		}
		temp=temp->next;
		i++;
	}
	//printf("EVICT: %d\n",frame);
	return frame;
}

/* This function is called on each access to a page to update any information
 * needed by the opt algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void opt_ref(pgtbl_entry_t *p) {
	struct table *temp = t;
	//goes through all elements in table
	while(temp!=NULL){
		if(temp->list != NULL){
			//if we find the frame we are looking for, remove it's index
			if(temp->list->index == Tcounter){
				temp->list = temp->list->next;
				temp->vaddr = p->frame >> PAGE_SHIFT;
			}
		}
		temp=temp->next;
	}
	Tcounter++;
	return;
}

/* Initializes any data structures needed for this
 * replacement algorithm.
 */
void opt_init() {

	//buffer for reading files 
	char buf[MAXLINE];
	// trace file pointer
	FILE *tfp = stdin;
	//counter of how many instructions we've read 
	Tcounter = 0;
	//checks if the file can be opened
	if(tracefile != NULL){
		if((tfp = fopen(tracefile, "r")) == NULL){
			exit(1);
		}
	}
	
	addr_t vaddr = 0;
	char type;
	//initialize table list
	t = malloc(sizeof(struct table));
	int index = 0;
	//read the tracefile 
	while(fgets(buf, MAXLINE, tfp) != NULL) {
		if(buf[0] != '=') {
			sscanf(buf, "%c %lx", &type, &vaddr);
			int address = vaddr >> PAGE_SHIFT;
			//insert each command into out virtual list
			insert_list(t, address, index);
			index++;
		} else {
			continue;
		}
	}
	
	cut_tail(t);
}

//removes unecessary tail of the table list
void cut_tail(struct table *t){
	struct table *p = t;
	if(p == NULL){
		return;
	}
	else if(p->next == NULL){
		p=NULL;
	}
	else{
		while(p->next->next != NULL){
			p = p->next;
		}
		p->next = NULL;
	}
	
}

//prints the table list with the linklists in each of it's elements
void print_future(struct table *t){
	printf("started printing\n");
	while(t!=NULL){
		printf("Address: %d\n",t->vaddr);
		struct Bnode *node = t->list;
		while(node!=NULL){
			printf("            %d\n",node->index);
			node = node->next;
		}
		t=t->next;
	}
}

