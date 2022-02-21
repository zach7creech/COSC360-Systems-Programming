/* Author: Zachery Creech
 * COSC360 Fall 2020
 * Lab2: l2p1.c
 * This program reads a stream of bytes from a file "converted" that represents a
 * list of ip addresses and associated names, translates the bytes, and stores the
 * data in a red-black tree. The user is then allowed to search through the database
 * repeatedly. This version uses buffered I/O functions such as fopen, fgetc, etc.
 * 09/14/2020 */

#include "jval.h"
#include "jrb.h"
#include "dllist.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct Computer 
{
	unsigned char* ip;
	int num_names;
	Dllist names;
} *Computer;

int main()
{
	FILE *f = fopen("converted", "r");
	JRB all_names = make_jrb();
	
	int i, j;
	char c;
	unsigned char* ip = malloc(4);
	
	//find all ip addresses with associated names
	while(!feof(f))
	{
		//create new computer struct with malloc
		int names = 0;
		Computer computer = malloc(sizeof(struct Computer));
		computer->ip = malloc(4);	
		computer->names = new_dllist();

		//get 4 bytes of ip
		for(i = 0; i < 4; i++)
			computer->ip[i] = fgetc(f);	

		//get 4 bytes of int
		char* hold_int = malloc(4);

		for(i = 0; i < 4; i++)
			hold_int[i] = fgetc(f);

		//convert unsigned chars into int with bitshifting
		for(i = 0; i < 4; i++)
		{		
			//"copy" 8 bits from char, then move 8 to the left to copy next part
			names |= hold_int[i];
			if(i != 3)
				names <<= 8;
		}

		computer->num_names = names;
		
		free(hold_int);

		//get each name
		for(i = 0; i < names; i++)
		{
			int name_size = 0;
			//save where beginning of current name is
			long name_beg = ftell(f);

			//if current name is an absolute name, these are for saving the prefix as local name
			int prefix_size = 0;
			bool need_prefix = false;
			long dot_mark = -1;
			char* prefix_name;
			
			//count size of name (and maybe prefix name) to malloc correctly
			while(1)
			{
				c = fgetc(f);
				name_size++;
				//if '.' is found then this name is an absolute name; set flag to save prefix
				if(c == '.' && dot_mark == -1)
				{	
					prefix_size = name_size;
					prefix_name = malloc(prefix_size);
					need_prefix = true;
					dot_mark = ftell(f);		
				}
				if(c == '\0')
					break;
			}

			char* new_name = malloc(name_size);

			fseek(f, name_beg, SEEK_SET);

			for(j = 0;; j++)
			{
				c = fgetc(f);
				new_name[j] = c;
				
				//if prefix needs to be saved, this is where separate creation of local name happens alongside absolute name
				if(ftell(f) <= dot_mark)
			{
					prefix_name[j] = c;
					//end of prefix has been reached, insert '\0' instead of '.' and finish prefix name
					if(ftell(f) == dot_mark)
					{
						prefix_name[j] = '\0';
						jrb_insert_str(all_names, prefix_name, new_jval_v(computer));
						dll_append(computer->names, new_jval_s(prefix_name));
					}
				}
				
				if(c == '\0')
					break;
			}

			jrb_insert_str(all_names, new_name, new_jval_v(computer));
			dll_append(computer->names, new_jval_s(new_name));
		}
	}

	//begin user input
	
	char* input = malloc(100);
	JRB find_node;
	JRB found;
	Dllist print;
	
	printf("Hosts all read in\n\n");
	
	//prompt continuously until EOF
	while(1)
	{
		printf("Enter host name: ");

		if(scanf("%s", input) == EOF)
			break;
	
		find_node = jrb_find_str(all_names, input);

		//if name exists, print ip and all names
		if(find_node != NULL)
		{
			jrb_traverse(found, all_names)
			{
				if(strcmp(input, found->key.s) == 0)
				{
					//print ip
					for(i = 0; i < 4; i++)
					{
						printf("%d", ((Computer)(found->val.v))->ip[i]);
						if(i < 3)
							printf(".");
					}
					printf(": ");

					//print names
					dll_traverse(print, ((Computer)(found->val.v))->names)
					{
						printf("%s ", print->val.s);
					}
					printf("\n\n");
				}
			}
		}
		else
			printf("no key %s\n\n", input);
	}
	
	//memory management
	
	JRB delete;
	Dllist node;
	
	//traverse entire tree
	jrb_traverse(delete, all_names)
	{
		Computer comp = (Computer)(delete->val.v);

		//reduce num_names by one to mark as visited, when 0 all names have been visited and computer pointer can be entirely freed
		comp->num_names--;
		if(comp->num_names == 0)
		{
			//free names in dllist
			dll_traverse(node, comp->names)
			{
				free(node->val.s);
			}
			free_dllist(comp->names);
			//free member variables
			free(comp->ip);
			free(comp);
		}
	}

	jrb_free_tree(all_names);

	//free misc memory and close file
	free(ip);
	free(input);

	fclose(f);

	return 0;
}
