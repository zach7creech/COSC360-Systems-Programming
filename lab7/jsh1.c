/* Author: Zachery Creech
 * COSC360 Fall 2020
 * Lab7: jsh1.c
 * This program creates a shell to execute commands without I/O redirection or piping. It uses fork() and wait(),
 * and it will only wait for a process to finish before executing the next if "&" is not specified at the end of
 * the command line. It takes one optional argument to change the prompt of the shell, defaulting to "jsh1: ".
 * 11/20/2020 */

#include "fields.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

int main(int argc, char** argv)
{
	char *prompt;
	int i, size, pid, status;
	char **newargv;
	bool duped = false;

	//decide prompt from optional command line argument
	if(argc > 1)
	{
		if(strcmp(argv[1], "-") == 0)
			prompt = "";
		else
		{	
			prompt = strdup(argv[1]);
			duped = true;
		}
	}
	else
		prompt = "jsh1: ";
	
	IS is = new_inputstruct(NULL);

	printf("%s", prompt);
	
	//continually ask for input until EOF
	while(get_line(is) >= 0)
	{
		//skip blank lines
		if(is->NF < 1)
		{	
			printf("%s", prompt);
			continue;
		}

		//end shell if "exit" command is given
		if(strcmp(is->fields[0], "exit") == 0)
			break;
		
		//save pid of child process
		pid = fork();
		
		//begin child process
		if(pid == 0)
		{	
			//check for "&" at end of command line for waiting, size of newargv only needs to be incremented if there is no "&"
			if(strcmp(is->fields[is->NF - 1], "&") == 0)
				size = is->NF;
			else
				size = is->NF + 1;
				
			newargv = (char **) malloc(sizeof(char *) * size);
			
			//copy over all fields
			for(i = 0; i < size; i++)	
				newargv[i] = is->fields[i];

			//set NULL terminator
			newargv[size - 1] = NULL;

			//execute shell command and handle memory and exiting if error occurs in execution
			execvp(newargv[0], newargv);
			perror(newargv[0]);
			free(newargv);
			jettison_inputstruct(is);
			exit(1);
		}
		else
		{
			//parent process should wait until the current child process finishes unless "&" is specified
			if(strcmp(is->fields[is->NF - 1], "&") != 0)
				while(wait(&status) != pid);
		}

		printf("%s", prompt);
	}	
	
	//when the shell exits, end zombie processes and clean up memory
	
	while(wait(&status) != -1);
	
	if(duped)
		free(prompt);
	
	jettison_inputstruct(is);

	return 0;
}
