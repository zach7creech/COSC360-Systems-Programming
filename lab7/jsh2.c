/* Author: Zachery Creech
 * COSC360 Fall 2020
 * Lab7: jsh2.c
 * This program creates a shell to execute commands and redirect their I/O, without piping. It uses fork(), dup2(), and
 * wait(), and it will only wait for a process to finish before executing the next if "&" is not specified at the end of
 * the command line. It takes one optional argument to change the prompt of the shell, defaulting to "jsh2: ".
 * 11/20/2020 */

#include "fields.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdbool.h>

int main(int argc, char** argv)
{
	char *prompt;
	int i, newi, size, pid, status, fd0, fd1;
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
		prompt = "jsh2: ";
	
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
		
		//size is the size of newargv from counting all command strings minus I/O redirection
		size = 0;

		//save pid of child process
		pid = fork();
		
		//begin child process
		if(pid == 0)
		{	
			//check every string in command line for I/O redirection and "&". If one isn't found at current index, increment size
			//any time I/O redirection is found, increment i to get the file name for redirection
			for(i = 0; i < is->NF; i++)
			{
				if(strcmp(is->fields[i], "<") == 0)
				{
					//check for input redirection, file error checking
					i++;
					fd0 = open(is->fields[i], O_RDONLY);
					if(fd0 < 0)
					{
						perror("jsh2: fd0");
						exit(1);
					}
					if(dup2(fd0, 0) != 0)
					{
						perror("jsh2: dup2(fd0, 0");
						exit(1);
					}
					close(fd0);
				}
				else if(strcmp(is->fields[i], ">") == 0)
				{
					//check for file output redirection for overwriting, file error checking
					i++;
					fd1 = open(is->fields[i], O_WRONLY | O_TRUNC | O_CREAT, 0644);
					if(fd1 < 0)
					{
						perror("jsh2: fd1");
						exit(1);
					}
					if(dup2(fd1, 1) != 1)
					{
						perror("jsh2: dup2(fd1, 1");
						exit(1);
					}
					close(fd1);
				}
				else if(strcmp(is->fields[i], ">>") == 0)
				{
					//check for file output redirection for appending, file error checking
					i++;
					fd1 = open(is->fields[i], O_WRONLY | O_APPEND | O_CREAT, 0644);
					if(fd1 < 0)
					{
						perror("jsh2: fd1");
						exit(1);
					}
					if(dup2(fd1, 1) != 1)
					{
						perror("jsh2: dup2(fd1, 1");
						exit(1);
					}
					close(fd1);
				}
				else if(strcmp(is->fields[i], "&") != 0)
					size++;
			}

			//increment size one more time for NULL terminator
			size++;

			newargv = (char **) malloc(sizeof(char *) * size);
			
			//newi increments independently of the for loop, only when a command string is NOT an I/O redirection for adding to newargv
			newi = 0;

			for(i = 0; i < is->NF; i++)	
			{	
				//add string to newargv if it isn't part of an I/O redirection, otherwise increment i and extra time to skip the redirection
				if(strcmp(is->fields[i], "<") != 0 && strcmp(is->fields[i], ">") != 0 && strcmp(is->fields[i], ">>") != 0 && strcmp(is->fields[i], "&") != 0)
					newargv[newi] = is->fields[i];
				else
					i++;
				newi++;
			}
			//set NULL terminator
			newargv[size - 1] = NULL;

			//execute parsed shell command and handle memory and exiting if error occurs in execution
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
