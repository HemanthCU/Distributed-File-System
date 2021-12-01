/* 
 * DFS.c - Distributed File Server Code (1 of 4)
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
#include <sys/stat.h>

#define LISTENQ  1024  /* second argument to listen() */
#define MAXREAD  80000

int open_listenfd(int port);
void *thread(void *vargp);
int checkCreds(char* uname, char* pass);

int dfsno;

int main(int argc, char **argv) {
    int listenfd, *connfdp, port, clientlen=sizeof(struct sockaddr_in);
    struct sockaddr_in clientaddr;
    pthread_t tid; 

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }
    port = atoi(argv[1]);
    dfsno = ((port-1) % 4) + 1;

    listenfd = open_listenfd(port);
    while (1) {
        connfdp = malloc(sizeof(int));
        *connfdp = accept(listenfd, (struct sockaddr*)&clientaddr, &clientlen);
        // Create a new thread only if a new connection arrives
        if (*connfdp >= 0)
            pthread_create(&tid, NULL, thread, connfdp);
    }
}

int checkCreds(char* uname, char* pass) {
    return 1;
}

/* thread routine */
void * thread(void * vargp) {
    int connfd = *((int *)vargp);
    pthread_detach(pthread_self());
    free(vargp);
    // Process the header to get details of request
    size_t n, m;
    int keepopen = 1;
    int msgsz;
    char buf[MAXREAD];
    char buf1[MAXREAD];
    char *resp = (char*) malloc (MAXREAD*sizeof(char));
    unsigned char *msg = (char*) malloc (MAXREAD*sizeof(char));
    char *context = NULL;
    char *comd;
    char *host;
    char *temp = NULL;
    char *tgtpath;
    char *fname = (char*) malloc (100*sizeof(char));
    char *folder = (char*) malloc (100*sizeof(char));
    char *httpver;
    char *uname;
    char *pass;
    FILE *fp;
    struct stat sb;
    while(keepopen) {
        keepopen = 0;
        n = read(connfd, buf, MAXREAD); // COMD UNAME PASS FNAME
        if ((int)n >= 0) {
            printf("Request received\n");
            comd = strtok_r(buf, " \t\r\n\v\f", &context);
            uname = strtok_r(NULL, " \t\r\n\v\f", &context);
            pass = strtok_r(NULL, " \t\r\n\v\f", &context);
            if (checkCreds(uname, pass)) {
                if (strcmp(comd, "GET") == 0) {
                    tgtpath = strtok_r(NULL, " \t\r\n\v\f", &context);
                } else if (strcmp(comd, "PUT") == 0) {
                    tgtpath = strtok_r(NULL, " \t\r\n\v\f", &context);
                    sprintf(fname,"./DFS%d/%s/%s", dfsno, uname, tgtpath);
                    fp = fopen(fname, "wb+");
                    m = 1;
                    while ((int)m > 0) {
                        m = read(connfd, buf1, MAXREAD); // DATA
                        fwrite(buf1, 1, m, fp);
                    }
                    fclose(fp);
                } else if (strcmp(comd, "LIST") == 0) {
                    
                } else if (strcmp(comd, "MKDIR") == 0) {
                    keepopen = 1;
                    sprintf(folder, "./DFS%d/%s", dfsno, uname);
                    if (!(stat(folder, &sb) == 0 && S_ISDIR(sb.st_mode))) {
                        if (mkdir(folder, 0777) == -1) {
                            //FAILED TO CREATE DIRECTORY
                            sprintf(resp, "FAILED TO CREATE DIRECTORY");
                        } else {
                            sprintf(resp, "SUCCESSFULLY CREATED DIRECTORY");
                        }
                    } else {
                        sprintf(resp, "DIRECTORY ALREADY EXISTS");
                    }
                    write(connfd, resp, strlen(resp));
                } else {
                    //INVALID COMMAND
                }
            } else {
                //WRONG CREDENTIALS
            }
        }
    }
    printf("Closing thread\n");
    close(connfd);
    return NULL;
}

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

