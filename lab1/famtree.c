/* Author: Zachery Creech
 * COSC360 Fall 2020
 * Lab1: famtree.c
 * This program reads input from stdin and parses it line by line using Dr. Plank's fields 
 * library. Each line is marked with a keyword that dictates what to do with the name that 
 * follows. The program creates Person structs to store each person and their relevant family 
 * connections, allowing for redundancy (i.e. when person is first created they are assigned
 * sex "male" and later another person has them listed as "father", implying being male again). 
 * After storing the input in a red-black tree, the program uses a type of breadth-first search
 * to print the people in order, parents first.
 * 09/07/2020 */

#include "fields.h"
#include "jval.h"
#include "dllist.h"
#include "jrb.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct Person 
{
	char* name;
	char* sex;
	struct Person* father;
	struct Person* mother;
	Dllist children;
	int printed;
	int visited;
} *Person;

char* get_name(IS is, bool isPerson);
int is_descendant(Person p);
int check_sex(IS is, Person p, char* sex);

int main()
{
	IS is;
	is = new_inputstruct(NULL);
	JRB people = make_jrb();
	JRB tmp;
	int i;
	Person p, subp;
	char* name;

	//begin reading input from stdin
	while(get_line(is) >= 0)
	{
		//skips lines that are only whitespace
		if(is->NF < 2)
			continue;

		//finds PERSON line, stores in Person pointer p
		if(strcmp(is->fields[0], "PERSON") == 0)
		{
			//if this person hasn't been created (not in the jrb tree), create and insert
			name = get_name(is, true);
			if(jrb_find_str(people, name) == NULL)
			{
				//initialize all members
				p = malloc(sizeof(struct Person));
				p->name = name;
				p->children = new_dllist();
				p->father = NULL;
				p->mother = NULL;
				p->sex = NULL;
				p->printed = 0;
				p->visited = 0;
				jrb_insert_str(people, name, new_jval_v(p));
			}
			else
			{	
				//if already created and in tree, get person from tree to be used for lines under PERSON keyword and free duplicate name
				p = jrb_find_str(people, name)->val.v;
				free(name);
			}
		}		
		else
		{
			//if a person comes up under PERSON keyword, also check if they exist and create and insert
			name = get_name(is, false);
			if(jrb_find_str(people, name) == NULL && strcmp(is->fields[0], "SEX") != 0)
			{
				//initialize members and store Person in subp pointer. p is PERSON, subp is person related to PERSON
				subp = malloc(sizeof(struct Person));
				subp->name = name;
				subp->children = new_dllist();
				subp->father = NULL;
				subp->mother = NULL;
				subp->sex = NULL;
				subp->printed = 0;
				subp->visited = 0;
				jrb_insert_str(people, name, new_jval_v(subp));
			}
			else if(strcmp(is->fields[0], "SEX") != 0)
			{
				//if already created and in tree, get person from tree
				subp = jrb_find_str(people, name)->val.v;
				free(name);
			}
			
			//begin checking sub keywords to assign family links and sex
			if(strcmp(is->fields[0], "FATHER_OF") == 0)
			{
				//error checking sex and duplicate parent
				if(check_sex(is, p, "Male"))
					return -1;
				if(subp->father != NULL && strcmp(subp->father->name, name) != 0)
				{
					fprintf(stderr, "Bad input -- child with two fathers on line %d\n", is->line);
					return -1;
				}
				//add subp as child of p, assign sex
				dll_append(p->children, new_jval_v(subp));
				p->sex = "Male";

				subp->father = p;
			}
			else if(strcmp(is->fields[0], "MOTHER_OF") == 0)
			{
				//error checking sex and duplicate parent
				if(check_sex(is, p, "Female"))
					return -1;
				if(subp->mother != NULL && strcmp(subp->mother->name, name) != 0)
				{
					fprintf(stderr, "Bad input -- child with two mothers on line %d\n", is->line);
					return -1;
				}
				//add subp as child of p, assign sex
				dll_append(p->children, new_jval_v(subp));
				p->sex = "Female";

				subp->mother = p;
			}
			else if(strcmp(is->fields[0], "FATHER") == 0)
			{
				//error checking sex and duplicate parent
				if(check_sex(is, subp, "Male"))
					return -1;
				if(p->father != NULL && strcmp(p->father->name, name) != 0)
				{
					fprintf(stderr, "Bad input -- child with two fathers on line %d\n", is->line);
					return -1;
				}
				//add p as child of subp, assign sex
				dll_append(subp->children, new_jval_v(p));
				subp->sex = "Male";

				p->father = subp;
			}
			else if(strcmp(is->fields[0], "MOTHER") == 0)
			{
				//error checking sex and duplicate parent
				if(check_sex(is, subp, "Female"))
					return -1;
				if(p->mother != NULL && strcmp(p->mother->name, name) != 0)
				{
					fprintf(stderr, "Bad input -- child with two mothers on line %d\n", is->line);
					return -1;
				}
				//add p as child of subp, assign sex
				dll_append(subp->children, new_jval_v(p));
				subp->sex = "Female";

				p->mother = subp;
			}
			else if(strcmp(is->fields[0], "SEX") == 0)
			{
				//check and assign sex
				if(strcmp(get_name(is, false), "M") == 0)
				{	
					if(check_sex(is, p, "Male"))
							return -1;
					p->sex = "Male";
				}
				else
				{	
					if(check_sex(is, p, "Female"))
						return -1;
					p->sex = "Female";
				}
			}
		}
	}

	//end of input, begin check for cycles
	Dllist ptr;
	
	//traverse entire tree, check each Person for cyclical relationship
	jrb_traverse(tmp, people) {
		if(is_descendant((Person)tmp->val.v))
		{	
			fprintf(stderr, "Bad input -- cycle in specification\n");
			return -1;
		}
	}
	
	//end cycle check, begin preparing for printing
	Dllist toprint = new_dllist();

	//traverse entire tree, add all people without parents to queue (dllist)
	jrb_traverse(tmp, people) {
		p = (Person) tmp->val.v;
		if(p->mother == NULL && p->father == NULL)
		{
			dll_append(toprint, new_jval_v(p));
		}
	}
	
	//begin breadth-first search, print each Person
	while(dll_empty(toprint) == 0)
	{
		//get first node, delete it from queue
		p = (Person)dll_first(toprint)->val.v;
		dll_delete_node(dll_first(toprint));
		if(p->printed == 0)
		{
			//if Person hasn't already been printed, and all valid parents have already been printed, format printing
			if((p->mother == NULL || p->mother->printed == 1) && (p->father == NULL || p->father->printed == 1))
			{
				//mark Person as printed
				p->printed = 1;
				//if any information is not available and NULL, print appropriate message
				char* father_name = "Unknown";
				char* mother_name = "Unknown";
				char* sex = "Unknown";

				if(p->father != NULL)
					father_name = p->father->name;
				if(p->mother != NULL)
					mother_name = p->mother->name;
				if(p->sex != NULL)
					sex = p->sex;
				
				//print with formatting
				printf("%s\n  Sex: %s\n  Father: %s\n  Mother: %s\n  Children:", p->name, sex, father_name, mother_name);
				
				if(dll_empty(p->children) == 1)
					printf(" None\n\n");
				else
				{
					//traverse all of Person's children and add them to the end of the queue for printing later, also print them here as children
					printf("\n");
					dll_traverse(ptr, p->children) {
						printf("    %s\n", ((Person)ptr->val.v)->name);
						dll_append(toprint, ptr->val);
					}
					printf("\n");
				}
			}
		}
	}

	return 0;
}

//processes name from each line, slightly modified version of c-style string reading from Dr. Plank's red-black tree lecture notes
//also can be used to extract 'M' or 'F' sex character
char* get_name(IS is, bool isPerson)
{
	int fullname_size = strlen(is->text1) - strlen(is->fields[0]) - 1; //name size including whitespace and \0 is the entire length of the line minus the tag length and \n
	//lines that correspond to subp, under PERSON keywords, are indented by two spaces
	if(!isPerson)
		fullname_size -= 2;
	
	//copy each part of name directly into allocated memory instead of using strcat
	char* name = malloc(fullname_size);
	strcpy(name, is->fields[1]);
	int name_size = strlen(is->fields[1]);

	int i;
	for(i = 2; i < is->NF; i++)
	{
		name[name_size] = ' ';
		strcpy(name + name_size + 1, is->fields[i]);
		name_size += strlen(name + name_size);
	}

	return name;
}

//depth-first search to verify that there aren't cycles in the tree, i.e. no person is their own parent
//from lab writeup
int is_descendant(Person p)
{
	Dllist ptr;
	if(p->visited == 1) return 0;

	if(p->visited == 2) return 1;
	p->visited = 2;
	dll_traverse(ptr, p->children) {
		if(is_descendant((Person)ptr->val.v)) return 1;
	}
	p->visited = 1;

	return 0;
}

//error checking for mismatched sex asssignment
int check_sex(IS is, Person p, char* sex)
{
	//if sex is already assigned, any further reference to Person's sex has to match original assignment
	if(p->sex != NULL && strcmp(p->sex, sex) != 0)
	{
		fprintf(stderr, "Bad input - sex mismatch on line %d\n", is->line);
		return 1;
	}
}
