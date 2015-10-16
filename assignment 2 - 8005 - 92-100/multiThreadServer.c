/*---------------------------------------------------------------------------------------
--      Source File:            multiThreadServer.c
--
--      Functions:              main
--
--      Date:                   January 22, 2014
--
--      Revisions:              (Date and Description)
--                                      
--      Designer:               Cole Rees and David Tran
--                              
--      Programmer:             Cole Rees and David Tran
--
--      Notes:
--      
--
--      To compile the application:
--                      
--            gcc -Wall -o multiThreadServer multiThreadServer.c -lpthread
--
--	To run the application:
--	
--		./multiThreadServer
---------------------------------------------------------------------------------------*/

#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define SERVER_TCP_PORT 7777 	//Default port
#define BUFLEN	1024	//Buffer length off the socket

//Structures
typedef struct host host;
struct host
{
	char*	status; //can be either "head", "connected", "active", "dead"
	char* 	ipAddress;
	int 	port;
	int		numOfRequest;
	int 	numOfBytesSent;
	struct host *next;
};

typedef struct
{
	char* 	ipAddress;
	int		socket;
	int port;
}clientData;


//Func Prototypes
int createBindSocket();
int listenForClients(int);
int startServer();
void* recieveFromClient(void*);
host* findEndOfList();
host* scanList(int, char*);
host* addToList(int, char*);
void deleteFromList(host *);
void printList();

//Globals
host *head;
int currentActiveHosts;
int maxActiveHosts;
FILE *fp;

pthread_mutex_t linkListLock = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char **argv) 
{	
	head = (host*)malloc(sizeof(host));
	head->status = "head";
	head->ipAddress = NULL;
	head->port = 0;
	head->numOfRequest = 0;
	head->numOfBytesSent = 0;
	head->next = NULL;

	maxActiveHosts = 0;
	currentActiveHosts = 0;
	
	//Start our server
	if(startServer() == -1)
	{
		perror("Server Exited");
		exit(1);
	}

	free(head);
	return EXIT_SUCCESS;
}

int startServer()
{
	int socket;

	socket = createBindSocket();
	if(socket == -1)
	{
		perror("Socket Error");
		return(-1);
	}
	
	if(listenForClients(socket) == -1)
	{
		perror("Client Connection Error");
		return(-1);
	}
	return 0;
}
//This functuion creates and binds a socket ot the global port number specified
//It the returns the socket.
int createBindSocket()
{
	int sd, port;
	struct	sockaddr_in server;

	//Get port from global default
	port = SERVER_TCP_PORT;

	// Create a stream socket
	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror ("Can't create a socket");
		return(-1);
	}

	// Bind an address to the socket
	bzero((char *)&server, sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	server.sin_addr.s_addr = htonl(INADDR_ANY); // Accept connections from any client

	if (bind(sd, (struct sockaddr *)&server, sizeof(server)) == -1)
	{
		perror("Can't bind name to socket");
		return(-1);
	}
	
	//return socket
	return sd;
}

int listenForClients(int sd)
{
	int	new_sd;
	socklen_t client_len;
	struct sockaddr_in client;

	//Listen for clients
	listen(sd, 5);
	while(1)
	{
		pthread_t clientThread;
		clientData *cl = malloc(sizeof(clientData));
		
		client_len = sizeof(client);
		
		if ((new_sd = accept (sd, (struct sockaddr *)&client, &client_len)) == -1)
		{
			fprintf(stderr, "Can't accept client\n");
			return -1;
		}
		cl->ipAddress 	= inet_ntoa(client.sin_addr);
		cl->socket 		= new_sd;
		cl->port		= client.sin_port;
		pthread_create(&clientThread, NULL, recieveFromClient, (void*)cl);
	}
	close(sd);
	return 1;
}

void* recieveFromClient(void *client) 
{
	clientData 	*cl = (clientData *)client;
	char* 		clientAddress;
	int 		fd, port;	
	host		*currentHost;

	fd 			= cl->socket;
	clientAddress 	= cl->ipAddress;
	port			= cl->port;
	

	currentHost = addToList(port, clientAddress);
	//printList();
	currentActiveHosts++;

	if (currentActiveHosts > maxActiveHosts)
	{
		maxActiveHosts = currentActiveHosts;
		printf("Maximum Connections Achieved: %d\n", maxActiveHosts);
	}
	
	while(1) {
		ssize_t count;
        char buf[BUFLEN];
        count = recv (fd, buf, sizeof buf, 0);
        if (count == -1)
        {
			/* If errno == EAGAIN, that means we have read all
                         data. So go back to the main loop. */
			if (errno != EAGAIN)
			{
				perror ("read");
			}
			break;
       	}
       	if (count == 0)
        {
			/* End of file. The remote has closed the
                         connection. */
			break;
		}
		
        /* Write the buffer to standard output */
		if(strcmp(buf, "END") == 0)
		{
			break;
		} else {
			//printf("Sending: %s\n", buf);
			currentHost->numOfRequest++;
			currentHost->numOfBytesSent += strlen(buf);
			send (fd, buf, BUFLEN, 0);
		}
	}

	deleteFromList(currentHost);
	currentActiveHosts--;
	
	close (fd);
	free(client);
	
	return NULL;
}

host* scanList(int port, char *ipAddr)
{
	host *match, *current;
 	match = head;
 	current = head->next;

	//pthread_mutex_lock(&linkListLock);
 	while ((current!=NULL)) {
		if ((current->port == port) && (strcmp(current->ipAddress, ipAddr) == 0)) {
			match = current;
			break;
		} else {
			current = current->next;
		}
 	}
 	
 	//pthread_mutex_unlock(&linkListLock);
 	return match;
}

host* findEndOfList()
{
	host *end, *current;
	end = head;
	current = head-> next;

	//pthread_mutex_lock(&linkListLock);
	while((current != NULL)) {
		if (current->next == NULL) {
			end = current;
			break;
		} else {
			current = current->next;
		}	
	}

	//pthread_mutex_unlock(&linkListLock);
	return end;
}

host* addToList(int port, char* ipAddr)
{
	host *node;
	//pthread_mutex_lock(&linkListLock);
	if (head->next == NULL)
	{
		node = malloc(sizeof(host));
		head->next = node;
		node->status = "connected";
		node->port = port;
		node->ipAddress = ipAddr;
		node->numOfRequest = 0;
		node->numOfBytesSent = 0;
		node->next = NULL;
	} else {
		host *previous = findEndOfList();
		node = malloc(sizeof(host));
		previous->next = node;
		node->status = "connected";
		node->port = port;
		node->ipAddress = ipAddr;
		node->numOfRequest = 0;
		node->numOfBytesSent = 0;
		node->next = NULL;
	}
	//pthread_mutex_unlock(&linkListLock);
	return node;
}

void deleteFromList(host *nodeToDelete)
{
	host *current = head;
	host *previous = NULL;
	
	//pthread_mutex_lock(&linkListLock);
	while(current != NULL)
	{
		if((nodeToDelete->port == current->port) && (strcmp(nodeToDelete->ipAddress, current->ipAddress) == 0))
		{
			host *temp = current;
			previous->next = current->next;
			fp = fopen("multiThread_log.csv", "a");
			fprintf(fp,  "%s,%d,%d,%d\n", temp->ipAddress, temp->port, temp->numOfRequest, temp->numOfBytesSent);
			fclose(fp);
			free(temp);
			//pthread_mutex_unlock(&linkListLock);
			return;
		} else {
			previous = current;
			current = current->next;
		}
	}
	//pthread_mutex_unlock(&linkListLock);
	return;
}

void printList()
{
    host *ptr = head;

    printf("\n -------Printing list Start------- \n");
    while(ptr != NULL)
    {
        printf("Socket: %d \n",ptr->port);
        ptr = ptr->next;
    }
    printf("\n -------Printing list End------- \n");

    return;
}
