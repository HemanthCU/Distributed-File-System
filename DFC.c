/* 
 * DFC.c - Distributed File Client Code
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>      /* for fgets */
#include <strings.h>     /* for bzero, bcopy */
#include <unistd.h>      /* for read, write */
#include <sys/socket.h>  /* for socket use */
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define LISTENQ  1024  /* second argument to listen() */
#define MAXREAD  80000

int open_listenfd(int port);
void performOp(int op);
char* tolowerstr(char* str);

int main(int argc, char **argv) {
    int op, *connfdp, port, clientlen=sizeof(struct sockaddr_in);
    char* oper = (char*) malloc (10 * sizeof(char));
    struct sockaddr_in clientaddr;
    pthread_t tid; 

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }
    op = 1;
    port = atoi(argv[1]);
    while(op >= 1 && op <= 3) {
        printf("Enter the operation:\n");
        scanf("%s", oper);
        if (strcmp(tolowerstr(oper), "put"))
        performOp(op);
    }
}

char* tolowerstr(char* oper) {
    char* operlower = (char*) malloc (10 * sizeof(char));
    size_t n = strlen(oper);
    int i;
    for (i = 0; i < (int)n; i++) {
        operlower[i] = tolower(oper[i]);
    }
    return operlower;
}

/* thread routine */
void performOp(int op) {
    int connfd;
    size_t n;
    int keepopen = 1;
    int msgsz; /* Size of data read from the file */
    char buf[MAXREAD]; /* Request full message */
    char *resp = (char*) malloc (MAXREAD*sizeof(char)); /* Response header */
    unsigned char *msg = (char*) malloc (MAXREAD*sizeof(char)); /* Data read from the file */
    char *context = NULL; /* Pointer used for string tokenizer */
    char *comd; /* Incoming HTTP command */
    char *host; /* Incoming HTTP host */
    char *temp = NULL; /* Temporary pointer to check if connection is persistent */
    char *tgtpath; /* Incoming HTTP URL */
    char *tgtpath1 = (char*) malloc (100*sizeof(char)); /* Path to the file referenced by URL */
    char *httpver; /* Incoming HTTP version */
    char *contType; /* Content type of the data being returned */
    char *postdata; /* Postdata to be appended to the request */
    char c;
    FILE *fp; /* File descriptor to open the file */
    while (keepopen) {
        n = read(connfd, buf, MAXREAD);
        if ((int)n >= 0) {
            printf("Request received\n");
        }
    }
    printf("Closing thread\n");
    close(connfd);
    return NULL;
}

/* 
 * open_sendfd - open and return a sending socket on port
 * Returns -1 in case of failure 
 */
int open_sendfd(int port, char *hostip) {
    int sendfd;
    struct sockaddr_in serveraddr;

    /* Create a socket descriptor */
    if ((sendfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;
    
    /* Sets a timeout of 10 secs. */
    /*struct timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec = 0;
    if (setsockopt(sendfd, SOL_SOCKET, SO_RCVTIMEO,
                    (struct timeval *)&tv,sizeof(struct timeval)) < 0)
        return -1;*/

    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET; 
    serveraddr.sin_addr.s_addr = inet_addr(hostip);
    serveraddr.sin_port = htons((unsigned short)port);
    if (connect(sendfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0)
        return -1;
    return sendfd;
} /* end open_sendfd */

/* 
 * open_listenfd - open and return a listening socket on port
 * Returns -1 in case of failure 
 */
int open_listenfd(int port) {
    int listenfd, optval=1;
    struct sockaddr_in serveraddr;
  
    /* Create a socket descriptor */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    /* Eliminates "Address already in use" error from bind. */
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, 
                    (const void *)&optval , sizeof(int)) < 0)
        return -1;

    /* Sets a timeout of 10 secs. */
    struct timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec = 0;
    if (setsockopt(listenfd, SOL_SOCKET, SO_RCVTIMEO,
                    (struct timeval *)&tv,sizeof(struct timeval)) < 0)
        return -1;

    /* listenfd will be an endpoint for all requests to port
       on any IP address for this host */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET; 
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    serveraddr.sin_port = htons((unsigned short)port); 
    if (bind(listenfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0)
        return -1;

    /* Make it a listening socket ready to accept connection requests */
    if (listen(listenfd, LISTENQ) < 0)
        return -1;
    return listenfd;
} /* end open_listenfd */

