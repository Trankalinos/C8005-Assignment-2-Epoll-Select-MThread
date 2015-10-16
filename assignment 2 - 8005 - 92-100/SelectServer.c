/*---------------------------------------------------------------------------------------
--  SOURCE FILE:    EpollServer.c
--
--  PROGRAM:        epollServer
--
--  FUNCTIONS:      static void SystemFatal (const char* message)
--                  void createSocket(int*)
--                  void bindSocket(int*, struct sockaddr_in*, int*)
--                  void SocketOptions(int*)
--                  void *serviceClient(void*)
--                  void ioworker(int *, int, int, int, fd_set, fd_set)
--                  static void SystemFatal(const char* message)
--
--  DATE:           February 16, 2014
--
--  DESIGNERS:      Jacob Miner
--
--  PROGRAMMERS:    Jacob Miner
--
--  NOTES:
--  Single threaded TCP echo server using select to service clients.
---------------------------------------------------------------------------------------*/
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <signal.h>

#define SERVER_PORT 7000
#define BUFLEN  80 
#define MAXLINE 4096
#define TRUE 1
#define THING 5000


int client[FD_SETSIZE];
int sockfd;
fd_set allset;
size_t i;

typedef struct
{
    int * client;
    int sockfd;
    fd_set allset;
    int i;
} SelectWrapper ;

void createSocket(int*);
void bindSocket(int*, struct sockaddr_in*, int*);
void SocketOptions(int*);
void *serviceClient(void*);
void ioworker(int *, int, int, int, fd_set, fd_set);
static void SystemFatal(const char* message);

int connected;


/*------------------------------------------------------------------------------
--
--  FUNCTION:    main
--
--  DATE:       February 16, 2014
--
--  DESIGNERS:  Jacob Miner  
--
--  PROGRAMMER: Jacob Miner 
--
--  INTERFACE:  main()
--
--  RETURNS:  int - 0 on success
--
--  NOTES: The main thread of the program. Calls all other functions.
--  
------------------------------------------------------------------------------*/
int main()
{
    //int client[FD_SETSIZE];
    int nready, /*sockfd = 0,*/ maxfd;
    int maxi = -1;
    int listenSocket, newSocket = 0;
    int port = SERVER_PORT;
    fd_set rset;
    //fd_set allset;
    i = 0;
    connected = 0;
    socklen_t clientLength = 0;    
    struct sockaddr_in server, client_addr;
    
    createSocket(&listenSocket);
    SocketOptions(&listenSocket);
    bindSocket(&listenSocket, &server, &port);
    
    listen(listenSocket, 5);
    maxfd = listenSocket;
    maxi = -1;
    
    for (i = 0; i < FD_SETSIZE; i++)
        client[i] = -1;
    
    FD_ZERO(&allset);
    FD_SET(listenSocket, &allset);

    while (TRUE)
    {
        rset = allset;
        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
    
        printf("servicing %d\n", connected);
        if (FD_ISSET(listenSocket, &rset)) /* New client connection */
        {
            clientLength = sizeof(client_addr);
            if ((newSocket = accept(listenSocket, (struct sockaddr *) &client_addr, &clientLength)) == -1)
                SystemFatal("accept error");
            
            connected++;
            for (i = 0; i < FD_SETSIZE; i++)
                if (client[i] < 0)
                {
                    client[i] = newSocket; // save descriptor
                    break;
                }
    
            if (i == FD_SETSIZE)
            {
                printf ("Too many clients\n");
                exit(1);
            }

            FD_SET (newSocket, &allset);     // add new descriptor to set
            if (newSocket > maxfd)
                maxfd = newSocket; // for select

            if (i > maxi)
                maxi = i;   // new max index in client[] array

            if (--nready <= 0)
                continue;   // no more readable descriptors
        }
        
        ioworker(client, sockfd, nready, maxi, rset, allset);
    }
    
    //free(info);
    close(listenSocket);
    return 0;
}

/*------------------------------------------------------------------------------
--
--  FUNCTION:    SystemFatal
--
--  DATE:       February 16, 2014
--
--  DESIGNERS:  Jacob Miner  
--
--  PROGRAMMER: Jacob Miner 
--
--  INTERFACE:  SystemFatal(const char* message)
--                          message - addition message to print with the error.
--
--  RETURNS:  void
--
--  NOTES: Prints the error stored in errno and aborts the program.
--  
------------------------------------------------------------------------------*/
static void SystemFatal(const char* message)
{
    perror (message);
    exit (EXIT_FAILURE);
}

/*------------------------------------------------------------------------------
--
--  FUNCTION:    ioworker
--
--  DATE:       February 16, 2014
--
--  DESIGNERS:  Jacob Miner  
--
--  PROGRAMMER: Jacob Miner 
--
--  INTERFACE:  ioworker(int *client, int sockfd, int nready, int maxi, fd_set rset, fd_set allset)
--                      client - the socket with the client connection
--                      sockfd - the file descriptors for the server
--                      nready - the number of clients ready
--                      maxi - maximum value to loop through
--                      rset - the fd set rset
--                      allset - the fd set allset
--
--  RETURNS:  void
--
--  NOTES: loops through the descriptors and checks to see which one has data.
--  
------------------------------------------------------------------------------*/
void ioworker(int *client, int sockfd, int nready, int maxi, fd_set rset, fd_set allset)
{
    size_t i = 0;
    SelectWrapper * info;
    
    for (i = 0; i <= maxi; i++)	/* check all clients for data */
	{
	    if ((sockfd = client[i]) < 0)
	    	continue;

	    if (FD_ISSET(sockfd, &rset))
 		{
 		
 		    info = malloc(sizeof(SelectWrapper));
 		    info->client = client;
 		    info->sockfd = sockfd;
 		    info->i      = i;
 		    info->allset = allset;
 		
            serviceClient((void*) info);
                
            if (--nready <= 0)
    	    	    break;
	    }
    }
}

/*------------------------------------------------------------------------------
--
--  FUNCTION:    serviceClient
--
--  DATE:       February 16, 2014
--
--  DESIGNERS:  Jacob Miner  
--
--  PROGRAMMER: Jacob Miner 
--
--  INTERFACE:  serviceClient(void *selectInfo)
--                      selectInfo - a struct with all the necessary variables to service the client 
--
--  RETURNS:  void
--
--  NOTES: Reads in client data, echoes it back to them, then closes the socket.
--  
------------------------------------------------------------------------------*/
void *serviceClient(void *selectInfo)
{
    int n = 0, bytes_to_read = 0;
    char *bp = 0;
    char buf[BUFLEN] = { 0 };
  
    SelectWrapper * info = (SelectWrapper *)selectInfo;
   
	bp = buf;
    bytes_to_read = BUFLEN;
    
    while ((n = read(info->sockfd, bp, bytes_to_read)) > 0)
    {
	    bp += n;
	    bytes_to_read -= n;
    }
    
    write(info->sockfd, buf, BUFLEN);
    
    if (n == 0)
    {
        
        close(info->sockfd);
        FD_CLR(info->sockfd, &info->allset);
        info->client[info->i] = -1;
        free(info);
    }
    
    return NULL;
}

/*------------------------------------------------------------------------------
--
--  FUNCTION:   createSocket
--
--  DATE:       February 16, 2014
--
--  DESIGNERS:  Jacob Miner  
--
--  PROGRAMMER: Jacob Miner 
--
--  INTERFACE: createSocket(int * listen_socket)
--                  listen_socket - the socket to listen on
--
--  RETURNS:  void
--
--  NOTES: Wrapper for socket(), that handles errors
--  
------------------------------------------------------------------------------*/
void createSocket(int * listen_socket)
{
    if ((*listen_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror ("Can't create a socket");
		exit(1);
	}
}

/*------------------------------------------------------------------------------
--
--  FUNCTION:   bindSocket
--
--  DATE:       February 16, 2014
--
--  DESIGNERS:  Jacob Miner  
--
--  PROGRAMMER: Jacob Miner 
--
--  INTERFACE: bindSocket(int * socket, struct sockaddr_in * server, int * port)
--                      socket - the socket to bind
--                      server - the server information
--                      port - the port to bind
--
--  RETURNS:  void
--
--  NOTES: Wrapper for bind that handles errors
--  
------------------------------------------------------------------------------*/
void bindSocket(int * socket, struct sockaddr_in * server, int * port)
{
    bzero((char *)server, sizeof(struct sockaddr_in));
	server->sin_family = AF_INET;
	server->sin_port = htons(*port);
	server->sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(*socket, (struct sockaddr *)server, sizeof(*server)) == -1)
	{
		perror("Can't bind name to socket");
		exit(1);
	}
}

/*------------------------------------------------------------------------------
--
--  FUNCTION:   SocketOptions
--
--  DATE:       February 16, 2014
--
--  DESIGNERS:  Jacob Miner  
--
--  PROGRAMMER: Jacob Miner 
--
--  INTERFACE: SocketOptions(int * socket)
--                      socket - the socket to set the options on
--
--  RETURNS:  void
--
--  NOTES: Sets relavent socket options, like reuse addr
--  
------------------------------------------------------------------------------*/
void SocketOptions(int * socket)
{
    int arg = 1;
    if (setsockopt (*socket, SOL_SOCKET, SO_REUSEADDR, &arg, sizeof(arg)) == -1)
    {
		perror("SetSockOpt Failed!");
		exit(1);
    }
}
