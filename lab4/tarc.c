/* Author: Zachery Creech
 * COSC360 Fall 2020
 * Lab4A: tarc.c
 * This program works similarly to "tar cf" by creating a tarfile from a directory 
 * and all of its contents. It takes a single argument on the command line that is
 * the absolute or relative pathname of the directory to copy. Then it recursively
 * traverses the given directory and all of its contents, writing the directory and
 * file information/content in bytes. The directory can be extracted by tarx.
 * 10/11/2020 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "jrb.h"
#include "dllist.h"

void rec_directory(char* fn, char* origin, JRB inodes);
char* get_suffix(char* full_path);

int main(int argc, char** argv)
{
	if(argc < 2)
	{
		fprintf(stderr, "usage: %s <pathname>\n", argv[0]);
		exit(1);
	}
	
	JRB inodes = make_jrb();
	char* origin = NULL;
	rec_directory(argv[1], origin, inodes);

	jrb_free_tree(inodes);

	return 0;
}

/* recursively traverse the given directory and all of its contents and write info to stdout
 * the majority of the program is executed here
 * based on Dr. Plank's Prsize lecture */
void rec_directory(char *fn, char* origin, JRB inodes)
{
	/* each call of this function represents a parent directory to be fully traversed
	 * near the end it is called again on all children directories
	 * when recursion ends, memory is freed at end of each recursive instance */
	DIR *d;
	struct dirent *de;
	struct stat buf;
	int exists, name_size;
	char *this_origin, *cur_path, *file_path;
	Dllist directories, tmp;
	FILE *f;
	
	d = opendir(fn);
	if(d == NULL)
	{
		perror("directory doesn't exist\n");
		exit(1);
	}

	/* origin represents the file path to be printed to stdout that is carried
	 * through from the root name and extended by one suffix from each directory 
	 * encountered. "this_origin" is the current path with the current directory
	 * suffix attached */
	char* suffix = get_suffix(fn);
	
	/* origin == NULL only on first function call, so this_origin just needs to
	 * be set to the suffix of the absolute path (the root directory) */
	if(origin == NULL)
	{	
		exists = lstat(fn, &buf);
		if(exists < 0)
		{
			fprintf(stderr, "Couldn't stat %s\n", fn);
		}
		else
		{	
			/* print root directory information before visiting its children
			 * every subsequent call is a parent directory, but its information has
			 * already been printed by its parent's traversal */
			this_origin = strdup(suffix);
			name_size = strlen(this_origin);
			fwrite(&name_size, 4, 1, stdout);
			fwrite(this_origin, 1, strlen(this_origin), stdout);
			fwrite(&buf.st_ino, sizeof(buf.st_ino), 1, stdout);
			jrb_insert_int(inodes, buf.st_ino, JNULL);
			fwrite(&buf.st_mode, sizeof(int), 1, stdout);
			fwrite(&buf.st_mtime, sizeof(buf.st_mtime), 1, stdout);
		}
	}
	else
	{
		//if this parent directory is not the root, update this_origin to reflect current directory path
		this_origin = (char*) malloc(sizeof(char)*(strlen(origin) + 258));
		sprintf(this_origin, "%s/%s", origin, suffix);
	}
	free(suffix);
	directories = new_dllist();
	//traverse current parent directory and print info on all children, save children directories to also examine
	for(de = readdir(d); de != NULL; de = readdir(d))
	{ 
		//current child directory/file has full path of fn + its name
		cur_path = (char*) malloc(sizeof(char)*(strlen(fn) + 258));
		sprintf(cur_path, "%s/%s", fn, de->d_name);
		//path to print is this_origin + its name
		file_path = (char*) malloc(sizeof(char)*(strlen(this_origin) + 258));
		sprintf(file_path, "%s/%s", this_origin, de->d_name);
		
		exists = lstat(cur_path, &buf);
		if(exists < 0)
		{
			fprintf(stderr, "Couldn't stat %s\n", cur_path);
		}
		//if current child exists and is not the "." or ".." self or parent references, print child's info
		else if(strcmp(de->d_name, ".") != 0 && strcmp(de->d_name, "..") != 0)
		{	
			//print the path size, the path, and the inode number for all children
			name_size = strlen(file_path);
			fwrite(&name_size, 4, 1, stdout);
			fwrite(file_path, 1, strlen(file_path), stdout);
			fwrite(&buf.st_ino, sizeof(buf.st_ino), 1, stdout);
	
			/* if this doesn't return NULL then the inode already exists, this child is a link and does not
			 * need the rest of its info printed */
			if(jrb_find_int(inodes, buf.st_ino) == NULL)
			{	
				jrb_insert_int(inodes, buf.st_ino, JNULL);

				//print the current child's mode and last modification time
				fwrite(&buf.st_mode, sizeof(int), 1, stdout);
				fwrite(&buf.st_mtime, sizeof(buf.st_mtime), 1, stdout);
				
				//if the current child is not a directory, print its file size and contents
				if(!S_ISDIR(buf.st_mode))
				{
					fwrite(&buf.st_size, sizeof(buf.st_size), 1, stdout);

					f = fopen(cur_path, "rb");
					if(f == NULL)
					{
						fprintf(stderr, "couldn't open file %s\n", de->d_name);
						exit(1);
					}
				
					char* content = malloc(buf.st_size);
					fread(content, 1, buf.st_size, f);
					fwrite(content, 1, buf.st_size, stdout);
					free(content);
					fclose(f);
				}
			}
		}
		//if current child is a directory, add it to list to traverse after all children are examined
		if(S_ISDIR(buf.st_mode) && strcmp(de->d_name, ".") != 0 && strcmp(de->d_name, "..") != 0)
		{
			dll_append(directories, new_jval_s(strdup(cur_path)));
		}
		//cur_path and file_path are both temporary strings for each child, so they must be freed at the end of each loop
		free(cur_path);
		free(file_path);
	}
	closedir(d);
	dll_traverse(tmp, directories)
	{
		//recursive call on every child directory in this parent directory. Saved path is not used for anything else, free it
		rec_directory(tmp->val.s, this_origin, inodes);
		free(tmp->val.s);
	}

	//recursion is over, free string and dllist that are unique to each recursive call
	free(this_origin);
	free_dllist(directories);
}

//gets suffix (last /* end of path) from absolute path
char* get_suffix(char* full_path)
{
	int i;
	int path_size = strlen(full_path);
	char* suffix;
	int suffix_size;
	
	//traverse full_path backwards until first '/' is found, then copy all characters after it
	for(i = path_size - 1; i >= 0; i--)
	{
		if(full_path[i] == '/')
		{
			i++;
			suffix_size = path_size - i;
			suffix = malloc(suffix_size + 1);
			memcpy(suffix, &full_path[i], suffix_size + 1);
			break;
		}
	}

	return suffix;
}
