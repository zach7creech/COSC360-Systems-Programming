/* Author: Zachery Creech
 * COSC360 Fall 2020
 * Lab7: jsh.c
 * This program creates a shell to execute commands and redirect their I/O, with piping. Any combination of "<" ">" ">>"
 * "|" and "&" will work. It uses pipe(), fork(), dup2(), and wait(), and it will only wait for a process to finish before 
 * executing the next if "&" is not specified at the end of the command line. It takes one optional argument to change the 
 * prompt of the shell, defaulting to "jsh: ".
 * 11/23/2020 */

#include "fields.h"
#include "jrb.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdbool.h>

int main(int argc, char** argv)
{
	char *prompt;
	int i, argi, size, num_commands, cur_com, found_commands, pid, status, fd0, fd1, pipe1[2], pipe2[2];
	char **newargv;
	JRB pids = make_jrb();
	JRB delete;

	//decide prompt from optional command line argument
	if(argc > 1)
	{
		if(strcmp(argv[1], "-") == 0)
			prompt = "";
		else
			prompt = argv[1];
	}
	else
		prompt = "jsh: ";
	
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
		
		//decide how many commands there are based on number of "|"
		num_commands = 1;

		for(i = 0; i < is->NF; i++)
			if(strcmp(is->fields[i], "|") == 0)
				num_commands++;

		//fork for each command separated by "|"
		for(cur_com = 1; cur_com <= num_commands; cur_com++)
		{
			//pipe1 is created on every "odd" command including the first
			//pipe2 is created on every "even" command, starting with the second
			if(cur_com % 2 != 0)
				pipe(pipe1);
			else
				pipe(pipe2);
			
			pid = fork();
			
			//will need to wait on this child if "&" is not specified, so add to tree
			if(pid != 0 && strcmp(is->fields[is->NF - 1], "&") != 0)	
				jrb_insert_int(pids, pid, JNULL);

			//begin child process
			if(pid == 0)
			{	
				//if it's the first command but not the only command, direct stdout to pipe1 write
				if(cur_com == 1 && num_commands != 1)
				{
					if(dup2(pipe1[1], 1) != 1)
					{
						perror("jsh: dup2(pipe1[1])");
						jrb_free_tree(pids);
						jettison_inputstruct(is);
						exit(1);
					}
				}
				//if it's an even command, direct stdin to pipe1 read
				else if(cur_com % 2 == 0)
				{
					if(dup2(pipe1[0], 0) != 0)
					{
						perror("jsh: dup2(pipe1[0])");
						jrb_free_tree(pids);
						jettison_inputstruct(is);
						exit(1);
					}
					//if it's not the last command, also direct stdout to pipe2 write
					if(cur_com != num_commands)
					{
						if(dup2(pipe2[1], 1) != 1)
						{
							perror("jsh: dup2(pipe2[1])");
							jrb_free_tree(pids);
							jettison_inputstruct(is);
							exit(1);
						}
					}
					close(pipe2[1]);
					close(pipe2[0]);
				}
				//if it's an odd command and not the first one, direct stdin to pipe2 read
				else if(cur_com % 2 != 0 && cur_com != 1)
				{
					if(dup2(pipe2[0], 0) != 0)
					{
						perror("jsh: dup2(pipe2[0])");
						jrb_free_tree(pids);
						jettison_inputstruct(is);
						exit(1);
					}
					close(pipe2[1]);
					close(pipe2[0]);
					//if it's not the last command, also direct stdout to pipe1 write
					if(cur_com != num_commands)
					{
						if(dup2(pipe1[1], 1) != 1)
						{
							perror("jsh: dup2(pipe1[1])");
							jrb_free_tree(pids);
							jettison_inputstruct(is);
							exit(1);
						}
					}
				}
				
				//always close pipe1
				close(pipe1[1]);
				close(pipe1[0]);

				//start at the first command
				found_commands = 1;
				size = 0;
				//check every string in command line for I/O redirection and "&". If one isn't found at current index, increment size
				//any time I/O redirection is found, increment i to get the file name for redirection
				for(i = 0; i < is->NF; i++)
				{
					//if the current command separated by pipes hasn't been reached, skip is->fields[i] until it's found
					if(strcmp(is->fields[i], "|") == 0)
						found_commands++;
					
					if(found_commands < cur_com)
						continue;
					else if(found_commands > cur_com)
						break;
					
					if(strcmp(is->fields[i], "<") == 0)
					{
						//check for input redirection, file error checking
						i++;
						fd0 = open(is->fields[i], O_RDONLY);
						if(fd0 < 0)
						{
							perror("jsh: fd0");
							jrb_free_tree(pids);
							jettison_inputstruct(is);
							exit(1);
						}
						if(dup2(fd0, 0) != 0)
						{
							perror("jsh: dup2(fd0, 0");
							jrb_free_tree(pids);
							jettison_inputstruct(is);
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
							perror("jsh: fd1");
							jrb_free_tree(pids);
							jettison_inputstruct(is);
							exit(1);
						}
						if(dup2(fd1, 1) != 1)
						{
							perror("jsh: dup2(fd1, 1");
							jrb_free_tree(pids);
							jettison_inputstruct(is);
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
							perror("jsh: fd1");
							jrb_free_tree(pids);
							jettison_inputstruct(is);
							exit(1);
						}
						if(dup2(fd1, 1) != 1)
						{
							perror("jsh: dup2(fd1, 1");
							jrb_free_tree(pids);
							jettison_inputstruct(is);
							exit(1);
						}
						close(fd1);
					}
					else if(strcmp(is->fields[i], "&") != 0 && strcmp(is->fields[i], "|") != 0)
						size++;
				}

				//increment size one more time for NULL terminator
				size++;

				newargv = (char **) malloc(sizeof(char *) * size);
			
				//argi increments independently of the for loop, only when a command string is NOT an I/O redirection for adding to newargv
				argi = 0;

				found_commands = 1;
				
				for(i = 0; i < is->NF; i++)	
				{	
					//if the current command separated by pipes hasn't been reached, skip is->fields[i] until it's been found
					if(strcmp(is->fields[i], "|") == 0)
						found_commands++;

					if(found_commands < cur_com)
						continue;
					else if(found_commands > cur_com)
						break;
					
					//add string to newargv if it isn't part of an I/O redirection, otherwise increment i an extra time to skip the redirection
					if(strcmp(is->fields[i], "|") != 0)
					{
						if(strcmp(is->fields[i], "<") != 0 && strcmp(is->fields[i], ">") != 0 && strcmp(is->fields[i], ">>") != 0 && strcmp(is->fields[i], "&") != 0)
							newargv[argi] = is->fields[i];
						else
							i++;
						argi++;
					}
				}
				//set NULL terminator
				newargv[size - 1] = NULL;

				//execute parsed shell command and handle memory and exiting if error occurs in execution
				execvp(newargv[0], newargv);
				perror(newargv[0]);
				free(newargv);
				jrb_free_tree(pids);
				jettison_inputstruct(is);
				exit(1);
			}
			else
			{
				//pipe1 should be closed by the parent on every even command, or if the last command is reached
				if(cur_com % 2 == 0 || cur_com == num_commands)
				{	
					close(pipe1[1]);
					close(pipe1[0]);
				}
				//pipe2 should be closed by the parent on every odd command or if the last command is reached, UNLESS it's the first command
				if((cur_com % 2 != 0 || cur_com == num_commands) && cur_com != 1)
				{
					close(pipe2[1]);
					close(pipe2[0]);
				}
			}
		}
		
		//parent process should wait until all child processes finish unless "&" is specified
		while(!jrb_empty(pids))
		{
			delete = jrb_find_int(pids, wait(&status));

			if(delete != NULL)
				jrb_delete_node(delete);
		}
		
		printf("%s", prompt);
	}	
	
	//when the shell exits, end zombie processes and clean up memory

	while(wait(&status) != -1);
	
	jrb_free_tree(pids);
	jettison_inputstruct(is);
	
	return 0;
}
