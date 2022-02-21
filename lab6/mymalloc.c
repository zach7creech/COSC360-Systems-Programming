/* Author: Zachery Creech
 * COSC360 Fall 2020
 * Lab6: mymalloc.c
 * This program implements the procedures defined in mymalloc.h. This includes my_malloc(), my_free(),
 * free_list_begin(), free_list_next(), and coalesce_free_list(). My_malloc() is a buffered interface
 * to sbrk() to allocate memory for a user program, working like malloc() does on most systems. The
 * program manages a linked list of "nodes" that point to heap memory allocated by sbrk() that is not
 * in use. Coalesce_free_list() combines nodes that are adjacent in memory using qsort().
 * 12/06/2020 */

#include "mymalloc.h"

typedef struct flist
{
	int size;
	struct flist *flink;
	struct flist *blink;
} *Flist;

Flist malloc_begin = NULL;

int address_compare();

void *my_malloc(size_t size)
{
	//allocated memory must be 8-byte aligned and have 8 extra bytes for bookkeeping
	int mod = size % 8;

	if(mod != 0)
		size += 8 - mod;

	size += 8;
		
	Flist f;
	char *node;
	int remaining;
	
	//this will loop at most one time, if there isn't already enough memory allocated for the request
	//also will always loop on the first call because there aren't any nodes yet
	while(1)
	{
		//search every free node in list for one with enough size for user request
		for(f = malloc_begin; f != NULL; f = f->flink)
		{
			//found a big enough node
			if(f->size >= size)
			{
				remaining = f->size - size;
				//there must be at least 12 bytes left to create another node out of it
				if(remaining >= 12)
				{
					node = (char *)f;
					node += size;
					
					//set up new free node that is "size" away from where it originally was
					((Flist)node)->size = remaining;
					((Flist)node)->flink = f->flink;
					((Flist)node)->blink = f->blink;

					//hook it into the list
					if(f->flink != NULL)
						f->flink->blink = (Flist)node;
					if(f->blink != NULL)
						f->blink->flink = (Flist)node;
			
					//if this is the first node in the list, set malloc_begin to be the newly created node of leftover memory
					if(f == malloc_begin)
						malloc_begin = (Flist)node;
				}
				//otherwise just give all of the node to the user
				else
				{
					//fix links to remove the node from the list
					if(f->flink != NULL)
						f->flink->blink = f->blink;
					if(f->blink != NULL)
						f->blink->flink = f->flink;
				
					//if this is the first node in the list, set malloc_begin to be the next node
					if(f == malloc_begin)
						malloc_begin = f->flink;
				}
				//set the size of the allocated memory and return it to the user
				f->size = size;
				node = (char *)f;
				return node + 8;
			}
		}

		//if the program gets here, there was either no nodes on the free list or
		//none of the nodes were big enough

		if(size > 8192)
		{
			//if the request is larger than 8192 just give all of it to the user, don't add to the free list
			node = (char *)sbrk(size);
			*(int *)node = size;
			return node + 8;
		}
		else
		{
			//create a new free node of size 8192 with my_free() to be used in the for loop above
			node = (char *)sbrk(8192);
			*(int *)node = 8192;
			my_free(node + 8);
		}
	
		//if the program gets here then a node needs to be divided
		//the for loop above should only execute once more before returning
	}
}

//creates a new free node from an allocated block pointed to by ptr
void my_free(void *ptr)
{
	ptr -= 8;

	//hook the new node into the front of the list
	((Flist)ptr)->flink = malloc_begin;
	((Flist)ptr)->blink = NULL;
	
	if(malloc_begin == NULL)
	{
		//if this new node is the only node, set it up as the new malloc_begin
		malloc_begin = (Flist)ptr;
		malloc_begin->flink = NULL;
		malloc_begin->blink = NULL;
	}
	else
	{
		//otherwise fix malloc_begin's links then make the new node malloc_begin
		malloc_begin->blink = (Flist)ptr;
		malloc_begin = (Flist)ptr;
	}
}

//accessor functions for Dr. Plank's gradescripts
void *free_list_begin()
{
	return malloc_begin;
}

void *free_list_next(void *node)
{
	return ((Flist)node)->flink;
}

//combines free nodes that are adjacent in memory into one
void coalesce_free_list()
{
	void **free_list;
	int i = 0, fsize = 0;
	Flist node;
	void *cur, *cur_comp, *next;
	
	//first, find the size of the free_list and copy it into an array
	
	for(node = malloc_begin; node != NULL; node = node->flink)
		fsize++;

	free_list = (void *)malloc(sizeof(void *) * fsize);
	
	for(node = malloc_begin; node != NULL; node = node->flink)
	{
		free_list[i] = node;
		i++;
	}

	//sort the array from smallest memory address to largest
	qsort(free_list, fsize, sizeof(void *), address_compare);

	/* starting from second to last node in the sorted array and moving backward, compare that node and the one
	 * immediately next to decide if they are adjacent. If they are, then fix the links and update size. Expanding
	 * backwards lets each size update naturally into the next, rather than traversing the sorted list forward and
	 * checking if the next node in the sorted list is one that was just absorbed by the last */

	for(i = fsize - 2; i >= 0; i--)
	{
		cur = free_list[i];
		cur_comp = cur;
		next = free_list[i + 1];

		//add the current node's size to itself to find the address of what might be the next free node's address
		//cur_comp exists just to preserve the value of cur, instead of directly using free_list[i]
		cur_comp += ((Flist)cur)->size;

		//if the next node's address is the same as the current node's + its size, then they are adjacent and should be coalesced
		if(cur_comp == next)
		{
			//update current node's size to be the sum of the coalesced nodes
			((Flist)cur)->size += ((Flist)next)->size;
			
			//fix links, if current node is absorbing malloc_begin then make it the new malloc_begin
			if(((Flist)next)->blink != NULL)
				((Flist)next)->blink->flink = ((Flist)next)->flink;
			else
				malloc_begin = ((Flist)next)->flink;

			if(((Flist)next)->flink != NULL)
				((Flist)next)->flink->blink = ((Flist)next)->blink;
		}
	}

	free(free_list);
}

//compare function for use in qsort(), list free nodes in order of ascending address number
int address_compare(const void *a, const void *b)
{
	return *(int *)a - *(int *)b;
}
