/* Author: Zachery Creech
 * COSC360 Fall 2020
 * LabA: chat_server.c
 * This program uses socketing and pthreads to run a chat server that allows clients to chat
 * with each other using nc or jtelnet. The names of chat rooms are specified with command
 * line arguments, and when a client connects to the server they are shown all chat rooms
 * and all active users in each room. The program uses mutexes to protect data structures
 * that are shared between all threads.
 * 12/03/2020 */

#include "dllist.h"
#include "jrb.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>

typedef struct chat_room
{
	char *name;
	Dllist inputs;
	Dllist clients;
	pthread_mutex_t lock;
	pthread_cond_t cond;
} *Room;

typedef struct client
{
	char *name;
	int fd;
	FILE *fdr, *fdw;
	Room room;
} *Client;

//tree is global so all threads can access it easily
JRB t;

void *client_thread();
void *chatroom_thread();

int main(int argc, char **argv)
{
	if(argc < 3)
	{
		fprintf(stderr, "usage: chat_server port Chat-Room-Names ...");
		return -1;
	}
	
	int i;
	pthread_t *tidp;
	Room room;
	t = make_jrb();

	//set up all rooms, start separate thread for each
	for(i = 2; i < argc; i++)
	{
		room = (Room) malloc(sizeof(struct chat_room));
		room->name = argv[i];
		room->inputs = new_dllist();
		room->clients = new_dllist();
		pthread_cond_init(&room->cond, NULL);
		pthread_mutex_init(&room->lock, NULL);
		tidp = (pthread_t *) malloc(sizeof(pthread_t));
		pthread_create(tidp, NULL, chatroom_thread, room);
		jrb_insert_str(t, argv[i], new_jval_v(room));
	}
	
	int fd, *fdp;
	int sock;

	sock = serve_socket(atoi(argv[1]));

	//continually look for clients to connect, when one is found start a thread for them
	while(1)
	{
		fd = accept_connection(sock);
		fdp = (int *) malloc(sizeof(int));
		*fdp = fd;
		tidp = (pthread_t *) malloc(sizeof(pthread_t));
		pthread_create(tidp, NULL, client_thread, fdp);
	}
	
	return 0;
}

void *client_thread(void *v)
{
	int *fdp;
	JRB tmp, room_node;
	Room room;
	Dllist member, delete;
	Client client;
	FILE *fin, *fout;
	char input[1000];
	char *output;
	char *name;
	char *room_name;
	char welcome[1000];
	char left[1000];

	//setup file pointers for the client
	fdp = (int *)v;
	fin = fdopen(*fdp, "r");
	if(fin == NULL) { perror("making fin"); exit(1); }
	fout = fdopen(*fdp, "w");
	if(fout == NULL) { perror("making fout"); exit(1); }

	//all fputs() and fflush() are contained within if statements to error check if client has disconnected
	
	if(fputs("Chat Rooms:\n\n", fout) == EOF)
	{
		close(*fdp);
		pthread_exit(NULL);
	}
	
	//print out all chat rooms and active users
	jrb_traverse(tmp, t)
	{
		if(fputs(tmp->key.s, fout) == EOF)
		{
			close(*fdp);
			pthread_exit(NULL);
		}
		if(fputs(":", fout) == EOF)
		{
			close(*fdp);
			pthread_exit(NULL);
		}
		dll_traverse(member, ((Room)(tmp->val.v))->clients)
		{
			if(fputs(" ", fout) == EOF)
			{
				close(*fdp);
				pthread_exit(NULL);
			}
			if(fputs(((Client)(member->val.v))->name, fout) == EOF)
			{
				close(*fdp);
				pthread_exit(NULL);
			}
		}
		if(fputs("\n", fout) == EOF)
		{
			close(*fdp);
			pthread_exit(NULL);
		}
	}

	//get name of new client
	if(fputs("\nEnter your chat name (no spaces):\n", fout) == EOF)
	{
		close(*fdp);
		pthread_exit(NULL);
	}
	if(fflush(fout) == EOF)
	{
		close(*fdp);
		pthread_exit(NULL);
	}
	if(fgets(input, 1000, fin) == NULL)
	{
		close(*fdp);
		pthread_exit(NULL);
	}
	name = malloc(strlen(input));
	memcpy(name, input, strlen(input));
	name[strlen(input) - 1] = '\0';

	//client chooses a room
	if(fputs("Enter chat room:\n", fout) == EOF)
	{
		close(*fdp);
		pthread_exit(NULL);
	}
	if(fflush(fout) == EOF)
	{
		close(*fdp);
		pthread_exit(NULL);
	}
	if(fgets(input, 1000, fin) == NULL)
	{
		close(*fdp);
		pthread_exit(NULL);
	}
	room_name = malloc(strlen(input));
	memcpy(room_name, input, strlen(input));
	room_name[strlen(input) - 1] = '\0';
	
	//check if the room exists, and if it does, set up the client and add it to the room
	//also signal the chatroom's thread to output the "client joined" message to all users in the room
	room_node = jrb_find_str(t, room_name);
	free(room_name);
	if(room_node != NULL)
	{
		room = (Room) room_node->val.v;
		client = (Client) malloc(sizeof(struct client));
		client->name = name;
		client->fd = *fdp;
		client->fdr = fin;
		client->fdw = fout;
		client->room = room;
		
		pthread_mutex_lock(&room->lock);
		dll_append(room->clients, new_jval_v(client));
		//save the client's node to easily delete from room's client list whenever the client leaves
		delete = room->clients->blink;
		sprintf(welcome, "%s has joined\n", client->name);
		dll_append(room->inputs, new_jval_s(strdup(welcome)));
		pthread_cond_signal(&room->cond);
		pthread_mutex_unlock(&room->lock);
	}
	else
	{
		close(*fdp);
		pthread_exit(NULL);
	}

	//continuously check for input from the client. If they disconnect, construct the appropriate message
	//and signal the chatroom thread to output the "client left" message
	while(1)
	{
		//client left
		if(fgets(input, 1000, fin) == NULL)
		{
			sprintf(left, "%s has left\n", name);
			pthread_mutex_lock(&room->lock);
			dll_append(room->inputs, new_jval_s(strdup(left)));
			close(*fdp);
			free(name);
			free(delete->val.v);
			dll_delete_node(delete);
			pthread_cond_signal(&room->cond);
			pthread_mutex_unlock(&room->lock);
			pthread_exit(NULL);
		}
		//client typed something into the chat
		else
		{
			output = malloc(strlen(name) + 3 + strlen(input));
			strcpy(output, name);
			strcat(output, ": ");
			strcat(output, input);
			pthread_mutex_lock(&room->lock);
			dll_append(room->inputs, new_jval_s(strdup(output)));
			free(output);
			pthread_cond_signal(&room->cond);
			pthread_mutex_unlock(&room->lock);
		}
	}
}

void *chatroom_thread(void *v)
{
	Room room;
	Dllist tmp;
	
	room = (Room)v;

	//continuously wait for input from clients, including entering, leaving, and chat messages
	while(1)
	{
		//output messages to all clients until the inputs list is empty
		while(!dll_empty(room->inputs))
		{
			for(tmp = room->clients->flink; tmp != room->clients;)
			{
				//try outputting message to each client in the room, if one has disconnected, close its output buffer
				//and delete it from this room's client list
				if(fputs(room->inputs->flink->val.s, ((Client)(tmp->val.v))->fdw) == EOF)
				{
					fclose(((Client)(tmp->val.v))->fdw);
					tmp = tmp->flink;
					dll_delete_node(tmp->blink);
				}
				else if(fflush(((Client)(tmp->val.v))->fdw) == EOF)
				{
					fclose(((Client)(tmp->val.v))->fdw);
					tmp = tmp->flink;
					free(tmp->blink->val.v);
					dll_delete_node(tmp->blink);
				}
				else
					tmp = tmp->flink;
			}
			dll_delete_node(room->inputs->flink);
		}
		//wait until new messages are received
		pthread_cond_wait(&room->cond, &room->lock);
	}
}
