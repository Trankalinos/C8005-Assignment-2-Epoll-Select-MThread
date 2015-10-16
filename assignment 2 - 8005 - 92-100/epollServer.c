/*---------------------------------------------------------------------------------------
--      Source File:            epollServer.c
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
--            gcc -Wall -o epollServer epollServer.c -lpthread
--
--	To run the application:
--	
--		./epollServer
---------------------------------------------------------------------------------------*/
#include <pthread.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <strings.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <signal.h>

#define EPOLL_QUEUE_LEN	100
#define BUFLEN		1024
#define SERVER_PORT	7777

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

// Function Prototypes
void* ClearSocket (void*);
void close_fd (int);
int createBindServer();
int startServer();
int listenForClients();
host* findEndOfList();
host* scanList(int, char*);
void addToList(int, char*);
void deleteFromList(host *);
void printList();

//Globals
host *head;
int currentActiveHosts;
int maxActiveHosts;
int fd_server;
FILE *fp;

pthread_mutex_t linkListLock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t scanLock = PTHREAD_MUTEX_INITIALIZER;

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
	struct sigaction act;

	// set up the signal handler to close the server socket when CTRL-c is received
        act.sa_handler = close_fd;
        act.sa_flags = 0;
        if ((sigemptyset (&act.sa_mask) == -1 || sigaction (SIGINT, &act, NULL) == -1))
        {
                perror ("Failed to set SIGINT handler");
                return(-1);
        }
	
	if(createBindServer() == -1)
	{
		perror("Socket Error");
		return(-1);
	}
	
	if(listenForClients() == -1)
	{
		perror("Listen Error");
		return(-1);
	}
	
	return 1;
}

//This functuion creates and binds a socket ot the global port number specified
//It the returns the socket.
int createBindServer()
{
	int arg, port;
	struct sockaddr_in addr;

	port = SERVER_PORT;
	// Create the listening socket
	fd_server = socket (AF_INET, SOCK_STREAM, 0);
    	if (fd_server == -1) 
	{
		perror("Socket Error");
		return(-1);
	}
    	
    	// set SO_REUSEADDR so port can be resused imemediately after exit, i.e., after CTRL-c
    	arg = 1;
    	if (setsockopt (fd_server, SOL_SOCKET, SO_REUSEADDR, &arg, sizeof(arg)) == -1) 
	{
		perror("Socket OPT Error");
		return(-1);
	}
	
	arg = 1;
    	if (setsockopt (fd_server, IPPROTO_TCP, TCP_NODELAY, &arg, sizeof(arg)) == -1) 
	{
		perror("Socket OPT Error");
		return(-1);
	}
    	
    	// Make the server listening socket non-blocking
    	if (fcntl (fd_server, F_SETFL, O_NONBLOCK | fcntl (fd_server, F_GETFL, 0)) == -1) 
	{
		perror("FNCTL Error");
		return(-1);
	}
    	
    	// Bind to the specified listening port
    	memset (&addr, 0, sizeof (struct sockaddr_in));
    	addr.sin_family = AF_INET;
    	addr.sin_addr.s_addr = htonl(INADDR_ANY);
    	addr.sin_port = htons(port);
    	if (bind (fd_server, (struct sockaddr*) &addr, sizeof(addr)) == -1) 
	{
		perror("Bind Error");
		return(-1);
	}
	return 1;
}

int listenForClients()
{
	int i, num_fds, fd_new, epoll_fd, arg;
pthread_t pid;
	static struct epoll_event events[EPOLL_QUEUE_LEN], event;
	struct sockaddr_in remote_addr;
	socklen_t addr_size = sizeof(struct sockaddr_in);

	// Listen for fd_news; SOMAXCONN is 128 by default
    	if (listen (fd_server, 10) == -1) 
	{
		perror("Listen Error");
		return(-1);
	}
    	
    	// Create the epoll file descriptor
    	epoll_fd = epoll_create(EPOLL_QUEUE_LEN);
    	if (epoll_fd == -1) 
	{
		perror("Epoll Create Error");
		return(-1);
	}
    	
    	// Add the server socket to the epoll event loop
    	event.events = EPOLLIN | EPOLLOUT | EPOLLET;
    	event.data.fd = fd_server;
    	if (epoll_ctl (epoll_fd, EPOLL_CTL_ADD, fd_server, &event) == -1) 
	{
		perror("Epoll CTL Error");
		return(-1);
	}
    	
	// Execute the epoll event loop
    	for(;;)
	{
		//struct epoll_event events[MAX_EVENTS];
		num_fds = epoll_wait (epoll_fd, events, EPOLL_QUEUE_LEN, -1);
		if (num_fds < 0) 
		{
			perror("Epoll Wait Error");
			return(-1);
		}

		for (i = 0; i != num_fds; i++) 
		{
	    		// Case 1: Error condition
	    		if ((events[i].events & EPOLLERR) == EPOLLERR 
				|| (events[i].events & EPOLLHUP) == EPOLLHUP 
				|| ((events[i].events & EPOLLIN) != EPOLLIN 
				&& (events[i].events & EPOLLOUT) != EPOLLOUT))  
				{

					
					if ( (errno == EBADF) 
					|| (errno == ESHUTDOWN)
					|| (errno ==  ENOTCONN))
					{
						printf("Event Error: %s : %d\n", strerror(errno), errno);
						if (epoll_ctl (epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, &event) == -1) 
						{
							perror("Epoll CTL Error");
							continue;
						}
						continue;
					} else {
						
						if(errno == 0) {
							continue;
						} else {
							printf("Bottom Error: %s : %d\n", strerror(errno), errno);
							if (epoll_ctl (epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, &event) == -1) 
							{
								perror("Epoll CTL Error");
								continue;
							}
							currentActiveHosts--;
						}
						continue;
					}
	    		}

	    		// Case 2: Server is receiving a connection request
	    		if (events[i].data.fd == fd_server) 
				{
					fd_new = accept (fd_server, (struct sockaddr*) &remote_addr, &addr_size);
					if (fd_new == -1) 
					{
						perror("accept");
						return -1;
					}

					addToList(remote_addr.sin_port, inet_ntoa(remote_addr.sin_addr));
					currentActiveHosts++;

					if (currentActiveHosts > maxActiveHosts)
					{
						maxActiveHosts = currentActiveHosts;
						printf("Maximum Connections Achieved: %d\n", maxActiveHosts);
					}
					//printList();
					//printf(" Remote Address:  %s\n", inet_ntoa(remote_addr.sin_addr));
					//printf(" Remote Port:  %d\n", remote_addr.sin_port);

					// Make the fd_new non-blocking
					if (fcntl (fd_new, F_SETFL, O_NONBLOCK | fcntl(fd_new, F_GETFL, 0)) == -1) 
					{
						perror("FCNTL Error");
						return(-1);
					}
					arg = 1;
    					if (setsockopt (fd_new, SOL_SOCKET, SO_REUSEADDR, &arg, sizeof(arg)) == -1) 
					{
						perror("Socket OPT Error");
						return(-1);
					}
					arg = 1;
					if (setsockopt (fd_new, IPPROTO_TCP, TCP_NODELAY, &arg, sizeof(arg)) == -1) 
					{
						perror("Socket OPT Error");
						return(-1);
					}
					
					// Add the new socket descriptor to the epoll loop
					event.events = EPOLLIN | EPOLLOUT | EPOLLET;
					event.data.fd = fd_new;
					if (epoll_ctl (epoll_fd, EPOLL_CTL_ADD, fd_new, &event) == -1) 
					{
						perror("Epoll CTL Error");
						return(-1);
					}
	    		} else { //Case 3: We got some data
					pthread_create(&pid, NULL, ClearSocket, &events[i].data.fd);
					pthread_detach(pid);
				}
		}
    	}
	free(events);
	close(fd_server);
	return 0;
}

void* ClearSocket (void *fileDesc) 
{
	int done = 0;
	int fd;
	socklen_t len;
	struct sockaddr_in addr;
	host *currentHost;
	len = sizeof addr;

	fd = *((int *) fileDesc);

	getpeername(fd, (struct sockaddr*) &addr, &len);
	currentHost = scanList(addr.sin_port, inet_ntoa(addr.sin_addr));
    while (1)
    {
		ssize_t count;
        char buf[BUFLEN];

        count = recv (fd, buf, sizeof buf, 0);
        if (count == -1)
        {
			/* If errno == EAGAIN, that means we have read all
                         data. So go back to the main loop. */
			//if (errno != EAGAIN)
			//{
				//perror ("read");
				return NULL;
                          	//done = 1;
			//}
			//continue;
       	}
       	if (count == 0)
        {
			/* End of file. The remote has closed the
                         connection. */
			done = 1;
			break;
	}
		
        /* Write the buffer to standard output */
	send (fd, buf, BUFLEN, 0);
	currentHost->numOfRequest++;
	currentHost->numOfBytesSent += strlen(buf);
	}

	if (done)
    {
		//printf ("Closed connection on descriptor %d\n", fd);

		/* Closing the descriptor will make epoll remove it
                     from the set of descriptors which are monitored. */
		//deleteFromList(currentHost);
		//printList();
		currentActiveHosts--;
		pthread_mutex_lock(&linkListLock);
		fp = fopen("epoll_log.csv", "a");
		fprintf(fp,  "%s,%d,%d,%d\n", currentHost->ipAddress, currentHost->port, currentHost->numOfRequest, currentHost->numOfBytesSent);
		fclose(fp);
		pthread_mutex_unlock(&linkListLock);
		//close(fd);
	}
	return NULL;
}

host* scanList(int port, char *ipAddr)
{
	host *match = malloc(sizeof (host)), *current= malloc(sizeof(host));
 	match = head;
 	current = head->next;

	//pthread_mutex_lock(&linkListLock);
 	while ((current!=NULL)) {
		if (current->port == port && (strcmp(current->ipAddress, ipAddr) == 0)) {
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

void addToList(int port, char* ipAddr)
{
	//pthread_mutex_lock(&linkListLock);
	if (head->next == NULL)
	{
		host *node;
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
		host *node;
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
}

void deleteFromList(host *nodeToDelete)
{
	host *current = head;
	host *previous = NULL;
	
	pthread_mutex_lock(&linkListLock);
	while(current != NULL)
	{
		if((nodeToDelete->port == current->port) && (strcmp(nodeToDelete->ipAddress, current->ipAddress) == 0) )
		{
			host *temp = current;
			previous->next = current->next;
			fp = fopen("epoll_log.csv", "a");
			fprintf(fp,  "%s,%d,%d,%d\n", temp->ipAddress, temp->port, temp->numOfRequest, temp->numOfBytesSent);
			fclose(fp);
			free(temp);
			pthread_mutex_unlock(&linkListLock);
			return;
		} else {
			previous = current;
			current = current->next;
		}
	}
	pthread_mutex_unlock(&linkListLock);
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
// close fd
void close_fd (int signo)
{
        close(fd_server);
	exit (EXIT_SUCCESS);
}
