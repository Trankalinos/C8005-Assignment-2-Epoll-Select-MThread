/*---------------------------------------------------------------------------------------
--	SOURCE FILE:	client.c - A robust tcp load testing client.
--
--	PROGRAM:		client
--
--	FUNCTIONS:		void createSocket(int*)
--					void* sendData(void*)
--					void createMessage(int)
--					void initSocket(struct sockaddr_in*, int*, struct hostent*, char*)
--					void connectSocket(int*, struct sockaddr_in*, struct hostent*)
--					void setArgs(int*, char**, char**, int*, int*, int*)
--					void updateStats()
--
--	DATE:			February 14, 2014
--
--	DESIGNERS:		Jacob Miner
--
--	PROGRAMMERS:	Jacob Miner
--
--	NOTES:
--	The program will establish a TCP connection to a user specifed server.
-- 	
---------------------------------------------------------------------------------------*/
#include <stdio.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_TCP_PORT		7000	// Default port
#define BUFLEN				80  	// Buffer length

char * message;
int count;
int loop;

void createSocket(int*);
void* sendData(void*);
void createMessage(int);
void initSocket(struct sockaddr_in*, int*, struct hostent*, char*);
void connectSocket(int*, struct sockaddr_in*);
void setArgs(int*, char**, char**, int*, int*, int*);
void updateStats();

/*------------------------------------------------------------------------------
--
--	FUNCTION:	main
--
--	DATE:		February 14, 2014
--
--	DESIGNERS:  Jacob Miner  
--
--	PROGRAMMER:	Jacob Miner 
--
-- 	INTERFACE: main(int argc, char **argv)
--
--	RETURNS:  int - 0 on success
--
--	NOTES: The main thread of the program. Calls all other functions.
--	
------------------------------------------------------------------------------*/
int main (int argc, char **argv)
{
	int messageSize = BUFLEN;
	int clients = 15000;
    loop = 1;
	int sd[clients], port;
	struct hostent *hp;
	struct sockaddr_in server;
	char  *host;
    pthread_t clientThread[clients];
    size_t i  = 0;

    setArgs(&argc, argv, &host, &port, &messageSize, &clients);

	createMessage(messageSize);

	for (i = 0; i < clients; i++)
	{
		createSocket(&sd[i]);
		initSocket(&server, &port, hp, host);
		connectSocket(&sd[i], &server);

		if (pthread_create(&clientThread[i], NULL, &sendData, (void *) &sd[i]) != 0)
	    {	
	        perror ("Can't create thread!");
	        exit(1);
	    }
    }

	getchar();

	printf("Finished!\n");
	free(message);

	return (0);
}

/*------------------------------------------------------------------------------
--
--	FUNCTION:	 setArgs
--
--	DATE:		February 14, 2014
--
--	DESIGNERS:  Jacob Miner  
--
--	PROGRAMMER:	Jacob Miner 
--
-- 	INTERFACE:  setArgs(int *argc, char** argv, char** host, int * port, int* messageSize, int * clients)
--						argc - pointer to the number of arguments
--						argv - pointer to the array of arguments
--						host - pointer to a string for the host name
--						port - pointer to the port variable, for specifying a port
--						messageSize - pointer to the int holding the message size
--						clients - pointer to the client threads
--
--	RETURNS:  void
--
--	NOTES: Sets the arguments for the program, based on argv from command line.
--	
------------------------------------------------------------------------------*/
void setArgs(int *argc, char** argv, char** host, int * port, int* messageSize, int * clients)
{
	switch(*argc)
	{
		case 2:
			*host =	argv[1];	// Host name
			*port =	SERVER_TCP_PORT;
		break;
		case 3:
			*host =	argv[1];
			*port =	atoi(argv[2]);	// User specified port
		break;
		case 4: 
			*host =	argv[1];
			*port =	atoi(argv[2]);	// User specified port
			*messageSize = atoi(argv[3]); // message size
		break;
		case 5: 
			*host =	argv[1];
			*port =	atoi(argv[2]);	// User specified port
			*messageSize = atoi(argv[3]);
			*clients = atoi(argv[4]); // threads
		break;
		case 6: 
			*host =	argv[1];
			*port =	atoi(argv[2]);	// User specified port
			*messageSize = atoi(argv[3]);
			*clients = atoi(argv[4]); // threads
			loop = atoi(argv[5]);
		break;
		default:
			fprintf(stderr, "Usage: %s [host] [port] [messageSize] [threads] [loops]\n", argv[0]);
			exit(1);
	}
}

/*------------------------------------------------------------------------------
--
--	FUNCTION:	 createMessage
--
--	DATE:		February 14, 2014
--
--	DESIGNERS:  Jacob Miner  
--
--	PROGRAMMER:	Jacob Miner 
--
-- 	INTERFACE:  createMessage(int messageSize)
--						messageSize - the size of the message to generate.
--
--	RETURNS:  void
--
--	NOTES: Creates a string of size messageSize to be sent through the socket.
--	
------------------------------------------------------------------------------*/
void createMessage(int messageSize)
{
	char alphabet = 0;
	size_t i = 0;

	message = calloc(messageSize, sizeof(char));

	printf("Creating message of size %d\n", messageSize);
	for (i = 0; i < (messageSize - 1); i++)
	{
		alphabet = (i % 26) + 97;
		strncat(message, &alphabet, 1);
	}
	alphabet = '\n';
	strncat(message, &alphabet, 1);
	printf("...Complete!\n");
}

/*------------------------------------------------------------------------------
--
--	FUNCTION:	 createSocket
--
--	DATE:		February 14, 2014
--
--	DESIGNERS:  Jacob Miner  
--
--	PROGRAMMER:	Jacob Miner 
--
-- 	INTERFACE:  createSocket(int * sd)
--						sd - pointer to the socket to create
--
--	RETURNS:  void
--
--	NOTES: Wrapper for socket(), that handles errors
--	
------------------------------------------------------------------------------*/
void createSocket(int * sd)
{
	if ((*sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("Cannot create socket");
		exit(1);
	}
}

/*------------------------------------------------------------------------------
--
--	FUNCTION:	 initSocket
--
--	DATE:		February 14, 2014
--
--	DESIGNERS:  Jacob Miner  
--
--	PROGRAMMER:	Jacob Miner 
--
-- 	INTERFACE:  initSocket(struct sockaddr_in *server, int * port, struct hostent *hp, char* host)
--						server - the structure with the server information
--						port - the port to connect on
--						hp - a structure for holding the host name
--						host - the ip address / host address to connect to
--
--	RETURNS:  void
--
--	NOTES: Wrapper that initializes the socket for further use.
--	
------------------------------------------------------------------------------*/
void initSocket(struct sockaddr_in *server, int * port, struct hostent *hp, char* host)
{
	bzero((char *)server, sizeof(struct sockaddr_in));
	server->sin_family = AF_INET;
	server->sin_port = htons(*port);
	if ((hp = gethostbyname(host)) == NULL)
	{
		fprintf(stderr, "Unknown server address\n");
		exit(1);
	}
	bcopy(hp->h_addr, (char *)&server->sin_addr, hp->h_length);
}

/*------------------------------------------------------------------------------
--
--	FUNCTION:	 connectSocket
--
--	DATE:		February 14, 2014
--
--	DESIGNERS:  Jacob Miner  
--
--	PROGRAMMER:	Jacob Miner 
--
-- 	INTERFACE:  connectSocket(int *sd, struct sockaddr_in *server, struct hostent *hp)
--						sd - socket to connect with
--						server - the server's information 
--
--	RETURNS:  void
--
--	NOTES: Wrapper for connect(), that handles errors
--	
------------------------------------------------------------------------------*/
void connectSocket(int *sd, struct sockaddr_in *server)
{
	if (connect (*sd, (struct sockaddr *)server, sizeof(*server)) == -1)
	{
		fprintf(stderr, "Can't connect to server\n");
		perror("connect");
		exit(1);
	}
}

/*------------------------------------------------------------------------------
--
--	FUNCTION:	sendData
--
--	DATE:		February 14, 2014
--
--	DESIGNERS:  Jacob Miner  
--
--	PROGRAMMER:	Jacob Miner 
--
-- 	INTERFACE: sendData(void * args)
--						args - a structure holding all the necessary variables
--							to send data to the server.
--
--	RETURNS:  void
--
--	NOTES: Function called in a new thread. Sends the message to the server, 
-- 		then waits for the server to echo it back.
--	
------------------------------------------------------------------------------*/
void* sendData(void * args)
{
	int n = 0, bytes_to_read = BUFLEN;
	char *bp, rbuf[BUFLEN];
	int sd = *((int *) args);
	size_t i =0;

	struct timeval start = { 0 }, end = { 0 };

	for (i = 0; i < loop; i++)
	{
	    gettimeofday(&start, NULL);
		send (sd, message, BUFLEN, 0);
		bp = rbuf;

		while ((n = recv (sd, bp, bytes_to_read, 0)) < BUFLEN)
		{
			bp += n;
			bytes_to_read -= n;
		}

		n = 0;
		bytes_to_read = BUFLEN;
		gettimeofday(&end, NULL);
		printf("%ld\n", (end.tv_usec - start.tv_usec));
	}

	//close(sd);
	return NULL;
}
