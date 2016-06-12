
const char * usage =
"                                                               \n"
"myhttpd <port> for iterative server				\n"         "myhttpd -f <port> for server with fork/processes		    \n"         "myhttpd -t <port> for server with multiple threads             \n"         "myhttpd -p <port> for server with pool of threads              \n";

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

pthread_mutex_t m1;
void zombieHandler(int sig);
int QueueLength = 5;
// Processes time request
void processRequestAndClose(int socket);
void processTimeRequest( int socket );
void nonConcurrent (int masterSocket);
void threadConcurrent (int masterSocket);
void processConcurrent (int masterSocket);
void threadPoolConcurrent (int masterSocket);
	int
main( int argc, char ** argv )
{
	char cMode;
	int port;
	// Print usage if not enough arguments
	if (argc == 3) {
		//fprintf(stderr, argv[1]);
		if(strcmp (argv[1], "-f") == 0) {
			cMode = 'f';
		}
		else if(strcmp (argv[1], "-t") == 0) {
			cMode = 't';
		}
		else if(strcmp (argv[1], "-p") == 0) {
			cMode = 'p';
		}
		else {
			fprintf( stderr, "%s", usage );
			exit( -1 );
		}
		// Get the port from the arguments
		port = atoi (argv[2]);
	}
	else if (argc == 2) {
		cMode = 'n';
		// Get the port from the arguments
		port = atoi (argv[1]);
	}
	else {
		fprintf( stderr, "%s", usage );
		exit( -1 );
	}
	signal(SIGCHLD, zombieHandler);

	// Set the IP address and port for this server
	struct sockaddr_in serverIPAddress; 
	memset( &serverIPAddress, 0, sizeof(serverIPAddress) );
	serverIPAddress.sin_family = AF_INET;
	serverIPAddress.sin_addr.s_addr = INADDR_ANY;
	serverIPAddress.sin_port = htons((u_short) port);

	// Allocate a socket
	int masterSocket =  socket(PF_INET, SOCK_STREAM, 0);
	if ( masterSocket < 0) {
		perror("socket");
		exit( -1 );
	}

	// Set socket options to reuse port. Otherwise we will
	// have to wait about 2 minutes before reusing the sae port number
	int optval = 1; 
	int err = setsockopt(masterSocket, SOL_SOCKET, SO_REUSEADDR, 
			(char *) &optval, sizeof( int ) );

	// Bind the socket to the IP address and port
	int error = bind( masterSocket,
			(struct sockaddr *)&serverIPAddress,
			sizeof(serverIPAddress) );
	if ( error ) {
		perror("bind");
		exit( -1 );
	}

	// Put socket in listening mode and set the 
	// size of the queue of unprocessed connections
	error = listen( masterSocket, QueueLength);
	if ( error ) {
		perror("listen");
		exit( -1 );
	}
	if (cMode == 'n')
		nonConcurrent (masterSocket);
	else if (cMode == 't') {
		threadConcurrent (masterSocket);
	}
	else if (cMode == 'f') {
		processConcurrent (masterSocket);
	}
	else if (cMode == 'p') {
		threadPoolConcurrent (masterSocket);
	}
}
void nonConcurrent (int masterSocket) {
	fprintf(stderr, "launching server in iterative mode\n");
	while ( 1 ) {

		// Accept incoming connections
		struct sockaddr_in clientIPAddress;
		int alen = sizeof( clientIPAddress );
		int slaveSocket = accept( masterSocket,
				(struct sockaddr *)&clientIPAddress,
				(socklen_t*)&alen);

		if ( slaveSocket < 0 ) {
			continue;
			perror( "accept" );
			exit( -1 );
		}

		// Process request.
		processTimeRequest( slaveSocket );

		// Close socket
		close( slaveSocket );
	}

}
void threadConcurrent (int masterSocket) {
	fprintf(stderr, "launching server with Threads\n");
	while ( 1 ) {

		// Accept incoming connections
		struct sockaddr_in clientIPAddress;
		int alen = sizeof( clientIPAddress );
		pthread_mutex_lock(&m1);
		int slaveSocket = accept( masterSocket,
				(struct sockaddr *)&clientIPAddress,
				(socklen_t*)&alen);
		pthread_mutex_unlock(&m1);
		if ( slaveSocket < 0 ) {
			continue;
			perror( "accept" );
			exit( -1 );
		}

		// Process request.
		pthread_t t;
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
		pthread_create(&t, &attr, (void * (*)(void *)) processRequestAndClose,
				(void *) slaveSocket);
	}

}
void processConcurrent (int masterSocket)  {
	fprintf(stderr, "launching server with Processes\n");
	while (1) {
		// Accept incoming connections
		struct sockaddr_in clientIPAddress;
		int alen = sizeof( clientIPAddress );
		int slaveSocket = accept( masterSocket,
				(struct sockaddr *)&clientIPAddress,
				(socklen_t*)&alen);
		if ( slaveSocket < 0 ) {
			continue;
			perror( "accept" );
			exit( -1 );
		}
		int ret = fork();
		if (ret == 0) {
			// Process request.
			processTimeRequest(slaveSocket);
			exit(1);
		}
		else if (ret < 0) {
			perror("fork");
			exit(-1);
		}
		// Close socket
		close( slaveSocket );
	}
}

void loopthread (int masterSocket) { 
	while (1) {
		struct sockaddr_in clientIPAddress;
		int alen = sizeof(clientIPAddress);
		pthread_mutex_lock(&m1);
		int slaveSocket = accept(masterSocket,
				(struct sockaddr *)&clientIPAddress,
				(socklen_t *)&alen);
		pthread_mutex_unlock(&m1);

		if (slaveSocket < 0) {
			continue;
			perror("accept");
			exit(-1);
		}


		// Handles the request and closes the socket
		processRequestAndClose(slaveSocket);
	} 
} 
void threadPoolConcurrent (int masterSocket) {
	fprintf(stderr, "launching server with Pool of Threads\n");
	pthread_t t[4];
	for (int i = 0; i < 4; i++) {
		pthread_create(&t[i], NULL, (void * (*)(void *))loopthread,
				(void *) masterSocket);
	}
	// Make use of main thread to also handle requests
	loopthread(masterSocket);

}
void processRequestAndClose(int slaveSocket ) {
	processTimeRequest( slaveSocket );
	// Close socket
	close( slaveSocket );
}
	void
processTimeRequest( int fd )
{
	int n;
	unsigned char newChar, oldChar = 0;
	int gotGet = 0;
	const int MAXSIZE = 1024;
	char *docpath = new char[MAXSIZE];
	char *curr_string = new char[MAXSIZE];
	int curr_string_len = 0;
	int docpath_seen = 0;
	memset(docpath, 0, MAXSIZE);
	memset(curr_string, 0, MAXSIZE);
	while (n = read(fd, &newChar, sizeof(newChar))) {
		curr_string_len++;
		if (newChar == ' ') {
			if (gotGet == 0) {
				gotGet = 1;
			} else if (docpath_seen == 0) {
				curr_string[curr_string_len - 1] = 0;
				strcpy(docpath, curr_string + 4);
				docpath_seen = 1;
			}
		} else if (oldChar == '\r' && newChar == '\n') {
			break;
		} else {
			oldChar = newChar;
			curr_string[curr_string_len - 1] = newChar;
		}
	}

	//fprintf(stderr,"url hit\n");

	delete curr_string;

	int max = 1024;
	char *buff = (char *)malloc(max * sizeof(char));
	int numRead = 0;
	while ((n = read(fd, &newChar, sizeof(newChar)))) {
		buff[numRead++] = newChar; 
		// Resize buffer if needed
		// Read until two consecutive <cr><lf> are read
		if (numRead >= max) {
			max = 2 * max;
			buff = (char *) realloc (buff, max);
		}
		if (oldChar == '\r' && newChar == '\n') {
			if (numRead > 3) {
				if (buff[numRead - 3] == '\n' && buff[numRead - 4] == '\r') {
					break;
				}
			}
		}
	}

	free(buff);


	//fprintf(stderr,"abhiga\n");
	char *cwd = {0};
	cwd = getcwd(cwd, 256);
	if ((strncmp (docpath, "/icons", strlen("/icons")) == 0) || (strncmp(docpath, "/htdocs", strlen("/htdocs")) == 0) || (strncmp(docpath, "/cgi-bin", strlen("/cgi-bin")) == 0)) {
		strcat(cwd, "/http-root-dir");
		strcat(cwd, docpath);
	}
	else if (strcmp(docpath, "/") == 0) {
		strcat(cwd, "/http-root-dir/htdocs/index.html");
	}
	else {
		strcat(cwd, "/http-root-dir/htdocs");
		strcat(cwd, docpath);
	}
	//fprintf(stderr,"%s\n", cwd);

	char content_Type[1000];
	memset(content_Type, 0, 1000);
	int gif = 0;
	if (strstr (cwd, ".html") != 0) {
		strcpy (content_Type, "text/html");
	}
	else if (strstr (cwd, ".gif") != 0) {
		strcpy (content_Type, "image/gif");
		gif = 1;
	}
	else {
		strcpy (content_Type, "text/plain");
	}
	//fprintf(stderr, "%s\n", content_Type);
	FILE * file;
	if (gif) {
		file = fopen(cwd, "rb");
	}
	else {
		file = fopen(cwd, "r");
	}
	if (file <= 0) {
		const char *notFound = "File not found.";
		write(fd, "HTTP/1.0", strlen("HTTP/1.0"));
		write(fd, " ", 1);
		write(fd, "404", 3);
		write(fd, "File", 4);
		write(fd, " ", 1);
		write(fd, "Not", 3);
		write(fd, " ", 1);
		write(fd, "Found,", 6);
		write(fd, "\r\n", 2);
		write(fd, "Server:", 7);
		write(fd, " ", 1);
		write(fd, "CS 252 lab5", strlen("CS 252 lab5"));
		write(fd, "\r\n", 2);
		write(fd, "Content-type:", 13);
		write(fd, " ", 1);
		write(fd, "text/plain", strlen("text/plain"));
		write(fd, "\r\n", 2);
		write(fd, "\r\n", 2);
		write(fd, notFound, strlen(notFound));
	}
	else {

		write(fd, "HTTP/1.0", strlen("HTTP/1.0"));
		write(fd, " ", 1);
		write(fd, "200 ", 4);
		write(fd, "Document", 8);
		write(fd, " ", 1);
		write(fd, "follows", 7);
		write(fd, "\r\n", 2);
		write(fd, "Server:", 7);
		write(fd, " ", 1);
		write(fd, "CS 252 lab5", strlen("CS 252 lab5"));
		write(fd, "\r\n", 2);
		write(fd, "Content-Type:", strlen("Content-Type:"));
		write(fd, " ", 1);
		write(fd, content_Type, strlen(content_Type));
		write(fd, "\r\n", 2);
		write(fd, "\r\n", 2);
		int count = 0;
		char ch;
		int f = fileno(file);
		while (count = read(f, &ch, sizeof(ch))) {
			write(fd, &ch, sizeof(ch));
		}
		fclose (file);
	}
}
void zombieHandler(int sig) {
	while (waitpid((pid_t)(-1), 0, WNOHANG) > 0) {}
}
