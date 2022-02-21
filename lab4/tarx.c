/* Author: Zachery Creech
 * COSC360 Fall 2020
 * Lab4B: tarx.c
 * This program works similarly to "tar xf" by recreating a directory and its contents from
 * a tarfile. It reads the tarfile from stdin and creates the files in order, filling files
 * (not directories) with content as necessary and setting their modification times. At the
 * end, all directories' modes and modification times are set.
 * 10/11/2020 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <utime.h>
#include <time.h>
#include "jrb.h"
#include "dllist.h"

/* all errors close files and free any memory that was allocated before the error check,
 * then calls this to free structures */
void free_error(JRB inodes, JRB d_modes, JRB d_times);

int main()
{
	JRB inodes = make_jrb();
	JRB d_modes = make_jrb();
	JRB d_times = make_jrb();
	JRB tmp;
	FILE *f;
	int path_size, mode;
	long inode, mtime, f_size;
	char *path, *dup_path, *content;
	struct timeval *times;

	/* when fread fails to read in a path_size properly, reached EOF (or error)
	 * read info for every directory/file in order of tarfile */
	while(fread(&path_size, sizeof(int), 1, stdin) == 1)
	{
		//read in path string after path size, add '\0' at the end
		path = malloc(path_size + 1);
		if(fread(path, 1, path_size, stdin) != path_size)
		{
			perror("given path size does not match existing path\n");
			free(path);
			free_error(inodes, d_modes, d_times);
			exit(1);
		}
		path[path_size] = '\0';	
		
		//read in inode number
		fread(&inode, sizeof(long), 1, stdin);
		
		//if inode number has been read before, skip reading the extra info, create hard link, and move to next file
		if(jrb_find_int(inodes, inode) == NULL)
		{	
			jrb_insert_int(inodes, inode, new_jval_s(strdup(path)));
			
			//read in mode, then modification time
			if(fread(&mode, sizeof(int), 1, stdin) != 1)
			{
				perror("couldn't read mode\n");
				free(path);
				free_error(inodes, d_modes, d_times);
				exit(1);
			}
			if(fread(&mtime, sizeof(long), 1, stdin) != 1)
			{
				perror("couldn't read mtime\n");
				free(path);
				free_error(inodes, d_modes, d_times);
				exit(1);
			}

			//modification time is stored in struct timeval with two members
			times = malloc(2 * sizeof(struct timeval));
			times[0].tv_sec = time(NULL);
			times[0].tv_usec = 0;
			times[1].tv_sec = mtime;
			times[1].tv_usec = 0;
			
			//directory mode and mtime must be set at the end, but create the directory
			if(S_ISDIR(mode))
			{
				//use the same string for both trees, free after traversing both
				dup_path = strdup(path);
				jrb_insert_str(d_modes, dup_path, new_jval_i(mode)); 
				jrb_insert_str(d_times, dup_path, new_jval_v(times));
				mkdir(path, 0777);
			}
			else
			{
				//otherwise its a file. Create it and read in its contents from tarfile
				f = fopen(path, "w");
				if(f == NULL)
				{
					perror("couldn't open file\n");
					free(path);
					free(times);
					free_error(inodes, d_modes, d_times);
					exit(1);
				}
				fread(&f_size, sizeof(long), 1, stdin);
				content = malloc(f_size);
				if(fread(content, 1, f_size, stdin) != f_size)
				{
					perror("couldn't read file contents\n");
					free(path);
					free(times);
					free(content);
					fclose(f);
					free_error(inodes, d_modes, d_times);
					exit(1);
				}
				fwrite(content, 1, f_size, f);
				fclose(f);
				free(content);
				//set file's mode and modification time
				chmod(path, mode);
				utimes(path, times);
				free(times);
			}
		}
		else
		{
			link(jrb_find_int(inodes, inode)->val.s, path);
		}
		free(path);
	}

	/* traverse all remaining data structs to free their contents, and to set mode and
	 * modification times for directories. Rather than call free_error to free memory
	 * separately and have to traverse the directory tree twice */
	
	jrb_traverse(tmp, inodes)
	{
		free(tmp->val.s);
	}
	
	jrb_traverse(tmp, d_modes)
	{
		chmod(tmp->key.s, tmp->val.i);
	}

	jrb_traverse(tmp, d_times)
	{
		utimes(tmp->key.s, tmp->val.v);
		free(tmp->key.s);
		free(tmp->val.v);
	}

	jrb_free_tree(inodes);
	jrb_free_tree(d_modes);
	jrb_free_tree(d_times);

	return 0;
}

//frees all data structs in case of error and exit
void free_error(JRB inodes, JRB d_modes, JRB d_times)
{
	JRB tmp;

	jrb_traverse(tmp, inodes)
	{
		free(tmp->val.s);
	}

	jrb_traverse(tmp, d_times)
	{
		free(tmp->key.s);
		free(tmp->val.s);
	}

	jrb_free_tree(inodes);
	jrb_free_tree(d_modes);
	jrb_free_tree(d_times);
}
