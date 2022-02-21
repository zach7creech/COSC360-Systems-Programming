/* Author: Zachery Creech
 * COSC360 Fall 2020
 * Lab3: fakemake.c
 * This program uses stat(2v) and system(3) to automate compiling of a single executable. It
 * reads compilation instructions (names of .c and .h files, flags, and libraries) from
 * description-file specified by argv[1]. First it stores the instructions, then it checks
 * all files using stat() to determine what needs to be recompiled based on the algorithm 
 * given in the lab write-up for Lab3. It builds relevant command strings and calls system() on 
 * them to execute compilation using gcc in the terminal. The program will exit upon encountering
 * an error and will output the relevant error message to stderr.
 * 09/26/2020 */

#include "jval.h"
#include "fields.h"
#include "dllist.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef struct Fakemake
{
	char* name;
	Dllist sources;
	Dllist headers;
	Dllist flags;
	Dllist libraries;
	Dllist objects;
} *Fakemake;

char* c_to_o(char* source);
char* compile(Fakemake fakemake, Dllist traverse, char type);
char* c_compile(Fakemake fakemake, Dllist source);
char* e_compile(Fakemake fakemake, Dllist next_string);
char* add_string(char* s1, char* s2);
void free_memory(IS is, Fakemake fakemake);

int main(int argc, char** argv)
{
	char* fakefile;
	
	//if no file is specified in command arguments, assume file is called "fmakefile"
	if(argc < 2)
		fakefile = "fmakefile";	
	else
		fakefile = argv[1];

	//for cleaning directory as shown in the writeup, but does not do anything for gradescripts
	if(strcmp(fakefile, "clean") == 0)
	{
		printf("rm -f core *.o f mysort\n");
		system("rm -f core *.o f mysort");
		return 0;
	}
	
	//opening input file and error checking
	IS is = new_inputstruct(fakefile);
	
	if(is == NULL)
	{
		fprintf(stderr, "fmakefile cannot open %s: No such file or directory\n", fakefile);
		return -1;
	}
	
	//create and allocate memory for new Fakemake object that stores strings for compiling new executable
	//program could be changed to create multiple executables at once by instantiating many Fakemake objects
	Fakemake fakemake = malloc(sizeof(struct Fakemake));
	
	//initialize all members of Fakemake object
	fakemake->name = NULL;
	fakemake->sources = new_dllist();
	fakemake->headers = new_dllist();
	fakemake->flags = new_dllist();
	fakemake->libraries = new_dllist();
	fakemake->objects = new_dllist();

	int i;
	
	//go through entire fmakefile and extract all instructions for compiling
	while(get_line(is) >= 0)
	{
		//skip whitespace lines
		if(is->NF < 2)
			continue;

		//add .c files
		if(strcmp(is->fields[0], "C") == 0)
		{
			for(i = 1; i < is->NF; i++)
				dll_append(fakemake->sources, new_jval_s(strdup(is->fields[i])));
		}
		//add .h files
		else if(strcmp(is->fields[0], "H") == 0)
		{
			for(i = 1; i < is->NF; i++)
				dll_append(fakemake->headers, new_jval_s(strdup(is->fields[i])));
		}
		//add executable name. Every fmakefile must contain exactly ONE E line
		else if(strcmp(is->fields[0], "E") == 0)
		{
			for(i = 1; i < is->NF; i++)
			{
				if(fakemake->name != NULL)
				{
					fprintf(stderr, "fmakefile (%d) cannot have more than one E line\n", is->line);
					return -1;
				}
				else
					fakemake->name = strdup(is->fields[i]);
			}
		}
		//add all flags
		else if(strcmp(is->fields[0], "F") == 0)
		{
			for(i = 1; i < is->NF; i++)
				dll_append(fakemake->flags, new_jval_s(strdup(is->fields[i])));
		}
		//add all libraries
		else if(strcmp(is->fields[0], "L") == 0)
		{
			for(i = 1; i < is->NF; i++)
				dll_append(fakemake->libraries, new_jval_v(strdup(is->fields[i])));
		}
	}
	
	//needs an executable name
	if(fakemake->name == NULL)
	{
		fprintf(stderr, "No executable specified\n");
		return -1;
	}
	
	//finished reading in all instructions
	
	//check all .h files to find the most recent one, error checking
	Dllist file;
	struct stat fileStat;

	long newest_header = -1;

	dll_traverse(file, fakemake->headers)
	{
		//if stat() returns < 0, file does not exist in current directory
		if(stat(file->val.s, &fileStat) < 0)
		{
			fprintf(stderr, "fmakefile: %s: No such file or directory\n");
			return -1;
		}
		else
		{
			if(fileStat.st_mtime > newest_header)
				newest_header = fileStat.st_mtime;
		}
	}

	//check all .c files and corresponding .o files according to algorithm in lab write-up
	Dllist source;
	int c_time = -1, e_time = -1;
	char* object;
	bool need_e = false, need_c = false;

	char* c_command = NULL;
	
	dll_traverse(source, fakemake->sources)
	{
		//create all object strings for comparison stat() to executable and possible compilation of executable later
		object = c_to_o(source->val.s);
		dll_append(fakemake->objects, new_jval_s(object));
		
		//error checking
		if(stat(source->val.s, &fileStat) < 0)
		{
			fprintf(stderr, "fmakefile: %s: No such file or directory\n", source->val.s);
			return -1;
		}
		else
		{
			//compare .o file to corresponding .c file and most recent header to decide if .c file needs to be recompiled
			c_time = fileStat.st_mtime;
			if(stat(object, &fileStat) < 0 || fileStat.st_mtime < c_time || fileStat.st_mtime < newest_header)
			{
				//set flags for relevant output and errors
				need_c = true;
				need_e = true;
				
				//get command to compile .c file, print it and execute command with system()
				c_command = compile(fakemake, source, 'c');
				printf("%s\n", c_command);
				if(system(c_command) != 0)
				{	
					fprintf(stderr, "Command failed.  Exiting\n");
					return -1;
				}
				free(c_command);
			}

		}
	}
	
	//check if executable already exists to set relevant flags
	if(stat(fakemake->name, &fileStat) < 0)
		need_e = true;
	else
		e_time = fileStat.st_mtime;
	
	Dllist cur_object;

	//compare executable time to object files, if any object is more recent then recompile executable
	dll_traverse(cur_object, fakemake->objects)
	{
		stat(cur_object->val.s, &fileStat);
		if(e_time < fileStat.st_mtime)
		{
			need_e = true;
			break;
		}
	} 

	//get command to compile executable, print it and execute command with system()
	if(need_e)
	{
		Dllist next_string;
		char* e_command = compile(fakemake, next_string, 'e');
		printf("%s\n", e_command);
		if(system(e_command) != 0)
		{
			fprintf(stderr, "Command failed.  Fakemake exiting\n");
			return -1;
		}
		free(e_command);
	}
	
	//no compilation needed
	if(!need_c && !need_e)
	{
		printf("%s up to date\n", fakemake->name);
		return -1;
	}
	
	free_memory(is, fakemake);
	
	return 0;
}

//creates string corresponding to .c file that ends in .o for string comparisons while checking with stat() and for executable compilation
char* c_to_o(char* source)
{
	char* object;
	int exten;

	object = strdup(source);
	//save what position extension 'c' is at, to replace with .o
	exten = strlen(object) - 1;
	object[exten] = 'o';

	return object;
}

//calls either c_compile or e_compile for .c file compilation or executable compilation respectively, specified by 'c' or 'e'
//after command is generated from all necessary strings, remove the whitespace left from calling add_string()
char* compile(Fakemake fakemake, Dllist traverse, char type)
{
	char* raw_command;

	if(type == 'c')
		raw_command = c_compile(fakemake, traverse);
	else if(type == 'e')
		raw_command = e_compile(fakemake, traverse);

	//remove trailing space left behind from add_string(), final version of string stored in fin_command
	char* fin_command = calloc(strlen(raw_command), sizeof(char));
	//copy everything over except \0, then replace the last char (which is whitespace ' ') with \0
	memcpy(fin_command, raw_command, strlen(raw_command) - 1);
	fin_command[strlen(fin_command)] = '\0';
	free(raw_command);

	return fin_command;
}

//constructs command to compile a .c file from all files necessary and in the specified order
char* c_compile(Fakemake fakemake, Dllist source)
{
	//all .c compilations begin with "gcc -c "
	char* command = malloc(8);
	strcpy(command,	"gcc -c ");
	
	//add all flags to the end of the string separated by whitespace
	Dllist flag;
	dll_traverse(flag, fakemake->flags)
	{
		command = add_string(command, flag->val.s);
	}
	
	//last string to be printed is the .c file name
	command = add_string(command, source->val.s);
	
	return command;
}

//constructs command to compile an executable from all strings necessary and in the specified order
char* e_compile(Fakemake fakemake, Dllist next_string)
{
	//all executable compilations begin with "gcc -o "
	char* command = malloc(8);
	strcpy(command, "gcc -o ");

	//first string after -o is the executable name
	command = add_string(command, fakemake->name);
	
	//then all flags
	dll_traverse(next_string, fakemake->flags)
	{
		command = add_string(command, next_string->val.s);
	}
	
	//then all .o files corresponding to all .c files
	dll_traverse(next_string, fakemake->objects)
	{
		command = add_string(command, next_string->val.s);
	}

	//then all libraries
	dll_traverse(next_string, fakemake->libraries)
	{
		command = add_string(command, next_string->val.s);
	}

	return command;
}

//adds a string and a trailing ' ' to another string, i.e. c++ s3 = s1 + s2 + ' '
char* add_string(char* s1, char* s2)
{
	//memory needed for combined string is the number of characters in both, plus 2. Adding 2 for \0 and one additional ' '
	int s3_size = strlen(s1) + strlen(s2) + 2;
	char* s3 = malloc(s3_size);
	
	//first copy s1 into the new string, then find the end of that string (\0) and add s2 starting at that position
	strcpy(s3, s1);
	strcpy(s3 + strlen(s1), s2);
	//add the new ' ' and \0 at the end
	strcpy(s3 + strlen(s1) + strlen(s2), " \0");
	
	//s1 is always the last version of command before another string is added, so it needs to be deleted now
	//s2 is from the Fakemake struct, which will be deleted by free_memory() at the end
	free(s1);

	return s3;
}

//frees all memory associated with the input file and strings stored within Fakemake struct
void free_memory(IS is, Fakemake fakemake)
{
	//free all members of fakemake
	//Dllists are traversed and the string stored in each node is deleted, then the Dllist itself is deleted
	Dllist tmp;
	
	free(fakemake->name);

	//sources
	dll_traverse(tmp, fakemake->sources)
	{
		free(tmp->val.s);
	}
	free_dllist(fakemake->sources);

	//headers
	dll_traverse(tmp, fakemake->headers)
	{
		free(tmp->val.s);
	}
	free_dllist(fakemake->headers);

	//flags
	dll_traverse(tmp, fakemake->flags)
	{
		free(tmp->val.s);
	}
	free_dllist(fakemake->flags);

	//libraries
	dll_traverse(tmp, fakemake->libraries)
	{
		free(tmp->val.s);
	}
	free_dllist(fakemake->libraries);
	
	//objects
	dll_traverse(tmp, fakemake->objects)
	{
		free(tmp->val.s);
	}
	free_dllist(fakemake->objects);
	
	free(fakemake);
	
	jettison_inputstruct(is);
}
