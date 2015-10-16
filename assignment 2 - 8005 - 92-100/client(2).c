/*---------------------------------------------------------------------------------------
--      Source File:            tcpClient.c
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
--            gcc -Wall -o multiThreadClient multiThreadClient.c
--
--	To run the application:
--	
--		./multiThreadClient -a <hostname> -b <numberOfThreadsToCreate> 
--		-c <numberOfTimesEachCLientReTransmits> -d <sizeOfEachMsgSent(1-79)>
---------------------------------------------------------------------------------------*/
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h> 
#include <netinet/tcp.h>
#include <time.h>
#include <gmp.h>
#include <inttypes.h>
#include <time.h>
#include <sys/time.h>
#include <fcntl.h> // for open
#include <math.h>

#define SERVER_TCP_PORT		7777	// Default port
#define THREADSTACK		65536  	//defeault thread stack
#define BUFLEN			1024  	// Buffer length
#define OPTIONS		"?a:b:c:"

int clientRun(char*, int, int);
void usage (char **);
int timeval_subtract(struct timeval *result, struct timeval *t2, struct timeval *t1);
unsigned myrandomlt26();

// Global
FILE *fp;

int main(int argc, char **argv)
{
	char* 	hostname;
	int		transmitNumber;
	int		sizeOfMsg;
	int 	opt;
	//Get our operator that have been passed in
	while ((opt = getopt(argc, argv, OPTIONS)) != -1)
	{
		switch (opt)
		{
			case 'a':
				hostname = optarg;
			break;

			case 'b':
				transmitNumber = atoi(optarg);
			break;

			case 'c':
				sizeOfMsg = atoi(optarg);
				//Error check 
			break;
			
			default:
				case '?':
					usage(argv);
			break;
		}
	}
	fp = fopen("./Client_Results.csv", "a");

	clientRun(hostname, transmitNumber, sizeOfMsg); 

	fclose(fp); 
	
	return 1;
}

int clientRun(char* hostname, int transmitNumber, int sizeOfMsg)
{
	int sd, port;
	struct hostent	*hp;
	struct sockaddr_in server;
	char  sbuf[BUFLEN];
	char* msg;
	char* temp;
	int i = 0, j = 0;
	int process = getpid();
	int serverError = 0;
	struct timeval tvBegin, tvEnd, tvDiff;
	

	//Get our necessary data
	port =	SERVER_TCP_PORT;
	temp = malloc(sizeof(char*));
	msg = malloc(sizeOfMsg * sizeof(char*));


	// Create the socket
	if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		perror("Cannot create socket");
		exit(1);
	}
	bzero((char *)&server, sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	if ((hp = gethostbyname(hostname)) == NULL)
	{
		fprintf(stderr, "Unknown server address\n");
		exit(1);
	}
	bcopy(hp->h_addr, (char *)&server.sin_addr, hp->h_length);

	// Connecting to the server
	if (connect (sd, (struct sockaddr *)&server, sizeof(server)) == -1)
	{
		fprintf(stderr, "Can't connect to server\n");
		perror("connect");
		exit(1);
	}

	struct sockaddr_in addr;
	socklen_t len = sizeof addr;
	
	getpeername(sd, (struct sockaddr*) &addr, &len);
	printf("Remote Address: %s\n", inet_ntoa(addr.sin_addr));
	printf("Remote Port: %d\n", addr.sin_port);
	
	for(i = 0; i < sizeOfMsg; i++)
	{
		sprintf(temp, "%c", ('A' + myrandomlt26()));
		strcat(msg, temp);
	}

	sprintf(sbuf, msg);
	for(;;)
	{
		ssize_t sndErr;
		gettimeofday(&tvBegin, NULL);
		sndErr = send (sd, sbuf, BUFLEN, MSG_DONTWAIT);
		serverError = errno;
		if(sndErr == -1)
		{
			printf ("Send Error: %s\n", strerror(serverError));
			if(serverError == ECONNRESET 
			|| serverError == ENOTCONN 
			|| serverError == EPIPE)
			{
				close(sd);
				break;
			} 
		}
		printf ("%d Sent Data: %s\n", process, sbuf);

		ssize_t rcvErr;
		char buf[BUFLEN];
		// rcvErr = recv(sd, buf, sizeof buf, MSG_DONTWAIT); // produces errors on recv,
		rcvErr = recv(sd, buf, sizeof(buf), 0);
		serverError = errno;
		if(rcvErr == -1)
		{
			printf ("Rcv Error: %s\n", strerror(serverError));
			if(serverError == ECONNRESET 
			|| serverError == ENOTCONN)
			{
				close(sd);
				break;	
			}
		} 
		if (rcvErr == 0)
		{
			printf ("Our Server Has Left Us.");
			close(sd);
			break;
		}
		if(strlen(buf))
		{
			printf("%d Received Data: %s\n", process, buf);
			j++;
			gettimeofday(&tvEnd, NULL);
			timeval_subtract(&tvDiff, &tvEnd, &tvBegin);
			fprintf(fp, "%s,%lu,%ld.%06ld\n", inet_ntoa(addr.sin_addr), strlen(sbuf), tvDiff.tv_sec, tvDiff.tv_usec);
			if(j >= transmitNumber)
				break;
		}
		sleep(5);
	}
	free(msg);
	free(temp);
	close(sd);
	return 0;	
}

/*----------  The timeval_subtract Function -------------*/
int timeval_subtract(struct timeval *result, struct timeval *t2, struct timeval *t1) {
	long int diff = (t2->tv_usec + 1000000 * t2->tv_sec) - (t1->tv_usec + 1000000 * t1->tv_sec);
	result->tv_sec = diff / 1000000;
	result->tv_usec = diff % 1000000;

	return (diff<0);
}

unsigned myrandomlt26() {
   long l;
   do { l = random(); } while (l>=(RAND_MAX/26)*26);
   return (unsigned)(l % 26);
}

// Usage Message
void usage (char **argv)
{
      fprintf(stderr, "Usage: %s -a <Hostname or IP Address> -b <Number of Threads to Create> -c <Number of Times Each Client Retransmits> -d <Size of Client Msg Sent>\n", argv[0]);
      exit(1);
}
