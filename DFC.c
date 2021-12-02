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
#include <ctype.h>

#define LISTENQ  1024  /* second argument to listen() */
#define MAXREAD  80000

int open_listenfd(int port);
int open_sendfd(int port, char *hostip);
void performOp(int op, char *fname);
char* toLowerStr(char* str);
int getHashVal(char* fname);

int main(int argc, char **argv) {
    int op, *connfdp, port, clientlen=sizeof(struct sockaddr_in);
    char* oper = (char*) malloc (10 * sizeof(char));
    char* fname = (char*) malloc (100 * sizeof(char));
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
        if (strcmp(toLowerStr(oper), "get") == 0) {
            printf("Enter the filename:\n");
            scanf("%s", fname);
            op = 1;
        } else if (strcmp(toLowerStr(oper), "put") == 0) {
            printf("Enter the filename:\n");
            scanf("%s", fname);
            op = 2;
        } else if (strcmp(toLowerStr(oper), "list") == 0) {
            strcpy(fname, "");
            op = 3;
        } else if (strcmp(toLowerStr(oper), "exit") == 0) {
            strcpy(fname, "");
            op = 4;
        } else {
            printf("\nInvalid operation\n");
        }
        if (op >= 1 && op <= 3)
            performOp(op, fname);
        else if (op == 4)
            return 0;
    }
    return 0;
}

int getHashVal(char* fname) {
    return 0;
}

char* toLowerStr(char* oper) {
    char* operlower = (char*) malloc (10 * sizeof(char));
    size_t n = strlen(oper);
    int i;
    for (i = 0; i < (int)n; i++) {
        operlower[i] = tolower(oper[i]);
    }
    return operlower;
}

/* thread routine */
void performOp(int op, char *fname) {
    int connfd;
    size_t n;
    int n1;
    int i, keepopen = 1;
    char buf[MAXREAD];
    char *resp = (char*) malloc (MAXREAD*sizeof(char));
    char *msg = (char*) malloc (MAXREAD*sizeof(char));
    char **msgpart = (char**) malloc (4*sizeof(char*));
    for (i = 0; i < 4; i++) {
        msgpart[i] = (char*) malloc (MAXREAD*sizeof(char));
        bzero(msgpart[i], MAXREAD);
    }
    char *context = NULL;
    char *tgtpath1 = (char*) malloc (100*sizeof(char));
    char *uname = (char*) malloc (100*sizeof(char));
    char *pass = (char*) malloc (100*sizeof(char));
    char *fname1 = (char*) malloc (100*sizeof(char));
    char c;
    FILE *fp;
    int dfsfd[4];
    int partsize[4];
    int x;

    //TODO: GET USER DETAILS
    strcpy(uname, "testuser");
    strcpy(pass, "testpass");

    for (i = 0; i < 4; i++) {
        dfsfd[i] = open_sendfd(10001 + i, "127.0.0.1");
    }

    if (op == 1) {
        //GET
    } else if (op == 2) {
        //PUT
        fp = fopen(fname, "rb+");
        n = fread(msg, 1, MAXREAD, fp);
        fclose(fp);
        n1 = (int) n;
        for (i = 0; i < 4; i++) {
            partsize[i] = (i == 3) ? n1 - (3 * (n1 / 4)) : n1 / 4;
            //SPLIT INTO PARTS
            sprintf(msgpart[i], "test %d", i);
        }

        //SEND MKDIR TO SERVERS
        bzero(resp, 100);
        sprintf(resp, "MKDIR %s %s", uname, pass);
        printf("%s\n", resp);
        for (i = 0; i < 4; i++) {
            write(dfsfd[i], resp, strlen(resp));
        }

        //GET RESPONSE FROM SERVERS
        for (i = 0; i < 4; i++) {
            bzero(buf, MAXREAD);
            n = read(dfsfd[i], buf, MAXREAD);
            if ((int)n < 0 || buf[0] == 'F') {
                printf("Failed to create directories\n");
                return;
            }
        }

        //SEND PUT AND DATA TO SERVERS
        for (i = 0; i < 4; i++) {
            dfsfd[i] = open_sendfd(10001 + i, "127.0.0.1");
            bzero(fname1, 100);
            sprintf(fname1, ".%s.%d", fname, i + 1);
            printf("%s\n", fname1);
            bzero(resp, 100);
            sprintf(resp, "PUT %s %s %s end", uname, pass, fname1);
            write(dfsfd[(i + x) % 4], resp, strlen(resp));
            write(dfsfd[(i + x) % 4], msgpart[i], strlen(msgpart[i]));

            dfsfd[i] = open_sendfd(10001 + i, "127.0.0.1");
            bzero(fname1, 100);
            sprintf(fname1, ".%s.%d", fname, ((i + 1) % 4) + 1);
            printf("%s\n", fname1);
            bzero(resp, 100);
            sprintf(resp, "PUT %s %s %s end", uname, pass, fname1);
            write(dfsfd[(i + x) % 4], resp, strlen(resp));
            write(dfsfd[(i + x) % 4], msgpart[(i + 1) % 4], strlen(msgpart[(i + 1) % 4]));
            printf("\n");
        }
    } else if (op == 3) {
        //LIST
    }
    printf("Closing thread\n");
    return;
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

