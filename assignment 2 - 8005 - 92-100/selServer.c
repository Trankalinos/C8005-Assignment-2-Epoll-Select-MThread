/*---------------------------------------------------------------------------------------
--	SOURCE FILE:		mux_svr.c -   A simple multiplexed echo server using TCP
--
--	PROGRAM:		mux.exe
--
--	FUNCTIONS:		Berkeley Socket API
--
--	DATE:			February 18, 2001
--
--	REVISIONS:		(Date and Description)
--				February 20, 2008
--				Added a proper read loop
--				Added REUSEADDR
--				Added fatal error wrapper function
--
--
--	DESIGNERS:		Based on Richard Stevens Example, p165-166
--				Modified & redesigned: Aman Abdulla: February 16, 2001
--
--				
--	PROGRAMMER:		Aman Abdulla
--
--	NOTES:
--	The program will accept TCP connections from multiple client machines.
-- 	The program will read data from each client socket and simply echo it back.
---------------------------------------------------------------------------------------*/
#include <fcntl.h>
#include <errno.h>
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

#define SERVER_TCP_PORT 7777
#define BUFLEN  1024 
#define MAXLINE 4096
#define TRUE 1
#define FALSE 0
#define LISTENQ 5

// Stucture Definitions
typedef struct {
	int *client;
	int sockfd;
	fd_set allset;
	int i;
	char* ipAddr;
	int port;
	int numOfRequests;
	int numOfBytesRecv;
} Sel_Struct;

typedef struct {
	int *client;
	int sockfd;
	int nready;
	int maxi;
	fd_set rset;
	fd_set allset;
	char* ipAddr;
	int port;
} ClientData;

// Globals
int client[FD_SETSIZE];
int sockfd;
int connected;
fd_set allset;
size_t i;
FILE *fp;

pthread_mutex_t linkListLock = PTHREAD_MUTEX_INITIALIZER;

// Function Prototypes
void SocketOptions(int*);
void *serviceClient(void*);
void *ioworker(void*);
// void ioworker(int *, int, int, int, fd_set, fd_set);
static void SystemFatal(const char* message);

int main() {
	int nready, maxfd, arg;
	int maxi = -1;
	int listen_sd, new_sd = 0;
	int port = SERVER_TCP_PORT;
	pthread_t clientThread;
	socklen_t client_len = 0;
	struct sockaddr_in server, client_addr;
	fd_set rset;
	i = 0;
	connected = 0;
	
	if ((listen_sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror ("Can't create a socket");
		exit(1);
	}

	arg = 1;
    if (setsockopt (listen_sd, SOL_SOCKET, SO_REUSEADDR, &arg, sizeof(arg)) == -1) {
		perror("SetSockOpt Failed!");
		exit(1);
    }

	bzero((char *)&server, sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	server.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(listen_sd, (struct sockaddr *)&server, sizeof(server)) == -1) {
		perror("Can't bind name to socket");
		exit(1);
	}

	listen(listen_sd, LISTENQ);
	maxfd = listen_sd;
	maxi = -1;

	for (i = 0; i < FD_SETSIZE; i++) client[i] = -1;

	FD_ZERO(&allset);
	FD_SET(listen_sd, &allset);
	

	while(TRUE) {
		rset = allset;
		nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
		
		if (FD_ISSET(listen_sd, &rset)) {
			client_len = sizeof(client_addr);
			if ((new_sd = accept(listen_sd, (struct sockaddr *)&client_addr, &client_len)) == -1)
				SystemFatal("accept error");
			
			connected++;
			printf("Currently serving %d connections...\n", connected);

			for (i = 0; i < FD_SETSIZE; i++) {
				if (client[i] < 0) {
					client[i] = new_sd;
					break;
				}
			}
			if (i == FD_SETSIZE) {
				printf("Too many clients :/\n");
				exit(1);
			}
			FD_SET(new_sd, &allset);
			if (new_sd > maxfd)
                maxfd = new_sd; // for select
            if (i > maxi)
                maxi = i;   // new max index in client[] array
            if (--nready <= 0)
                continue;   // no more readable descriptors
		}
		ClientData *aClient;
		aClient = malloc(sizeof(ClientData));
		aClient->client = &client[i];
		aClient->sockfd = sockfd;
		aClient->nready = nready;
		aClient->maxi = maxi;
		aClient->rset = rset;
		aClient->allset = allset;
		aClient->ipAddr = inet_ntoa(client_addr.sin_addr);
		aClient->port = client_addr.sin_port;
		pthread_create(&clientThread, NULL, ioworker, (void*)aClient);	
		pthread_detach(clientThread);
		// ioworker(client, sockfd, nready, maxi, rset, allset);
		// printf("Breakpoint 1");
	}
	close(listen_sd);
	return EXIT_SUCCESS;
}

//void ioworker(int *client, int sockfd, int nready, int maxi, fd_set rset, fd_set allset) {
void* ioworker(void *newClient) {

	size_t i = 0;
	Sel_Struct *data;
	ClientData *thisClient = (ClientData *)newClient;
	int *client = thisClient->client;
	int sockfd = thisClient->sockfd;
	int nready = thisClient->nready;
	int maxi = thisClient->maxi;
	fd_set rset = thisClient->rset;
	fd_set allset = thisClient->allset;

	char* clientAddr = thisClient->ipAddr;
	int clientPort = thisClient->port;

	for (i = 0; i <= maxi; i++) {
		if ((sockfd = client[i]) < 0) continue;

		if (FD_ISSET(sockfd, &rset)) {
			data = malloc(sizeof(Sel_Struct));
			data->client = client;
			data->sockfd = sockfd;
			data->i		 = i;
			data->allset = allset;
			data->ipAddr = clientAddr;
			data->port 	 = clientPort;
			data->numOfRequests = 0;
			data->numOfBytesRecv = 0;

			serviceClient((void *)data);

			if (--nready <= 0) break;
		}
	}
	 return NULL;	
}

void *serviceClient(void *selectData) {
	// int n = 0, bytes_to_read = 0;
	// char *bp = 0;
	char buf[BUFLEN];
	int done;
	ssize_t count;

	Sel_Struct *data = (Sel_Struct *)selectData;
	done = 0;
	while(TRUE) {
		count = recv(data->sockfd, buf, sizeof(buf), 0);
		
		printf("Received: %s\n", buf);
		if (count == -1) {
			if (errno != EAGAIN) {
				perror("read");
				done = 1;
			}
			break;
		}	

		if (count == 0) {
			done = 1;
			break;	
		}

		if (strcmp(buf, "END") == 0) {
			done = 1;
			break;
		} else {
			send(data->sockfd, buf, BUFLEN, 0);
			data->numOfBytesRecv += strlen(buf);
			data->numOfRequests++;
	   		printf("Sending: %s...\n", buf);
		}
	}

	if (done) {
		// printf("Closed connection on descriptor %d\n", data->sockfd);
		connected--;
		/*pthread_mutex_lock(&linkListLock);
			fp = fopen("select_log.csv", "a");
			fprintf(fp, "%s,%d,%d,%d\n", data->ipAddr, data->port, data->numOfRequests, data->numOfBytesRecv);
			fclose(fp);
		pthread_mutex_unlock(&linkListLock);*/
		close(data->sockfd);
		FD_CLR(data->sockfd, &data->allset);
		data->client[data->i] = -1;
		free(data);
	}

    return NULL;
}

static void SystemFatal(const char* message)
{
    perror (message);
    exit (EXIT_FAILURE);
}
