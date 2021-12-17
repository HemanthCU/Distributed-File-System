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
#include <dirent.h> 

#define LISTENQ  1024  /* second argument to listen() */
#define MAXREAD  200000

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
    FILE *fp;
    size_t n;
    char *confdata = (char*) malloc (MAXREAD*sizeof(char));
    char *context = NULL;
    char *temp, *temp1;

    bzero(confdata, MAXREAD);
    fp = fopen("dfs.conf", "rb+");
    n = fread(confdata, 1, MAXREAD, fp);
    fclose(fp);
    temp = strtok_r(confdata, " \t\r\n\v\f", &context);
    if (temp == NULL)
        return 0;
    temp1 = strtok_r(NULL, " \t\r\n\v\f", &context);
    if (temp1 == NULL)
        return 0;
    if (strcmp(temp, uname) == 0 && strcmp(temp1, pass) == 0)
            return 1;
    while (temp != NULL && temp1 != NULL) {
        temp = strtok_r(NULL, " \t\r\n\v\f", &context);
        if (temp == NULL)
            return 0;
        temp1 = strtok_r(NULL, " \t\r\n\v\f", &context);
        if (temp == NULL)
            return 0;
        if (strcmp(temp, uname) == 0 && strcmp(temp1, pass) == 0)
            return 1;
    }
    return 0;
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
    int fcount;
    char buf[MAXREAD];
    char buf1[MAXREAD];
    char *resp = (char*) malloc (MAXREAD*sizeof(char));
    unsigned char *msg = (char*) malloc (MAXREAD*sizeof(char));
    char *context = NULL;
    char *comd;
    char *host;
    char *temp = NULL;
    char *temp1 = (char*) malloc (100*sizeof(char));
    char *tgtpath;
    char *fname = (char*) malloc (100*sizeof(char));
    char *dirname = (char*) malloc (100*sizeof(char));
    char *folder = (char*) malloc (100*sizeof(char));
    char *httpver;
    char *uname;
    char *pass;
    char *keep;
    FILE *fp;
    DIR *d;
    struct dirent *dir;
    struct stat sb;
    while(keepopen) {
        keepopen = 0;
        bzero(buf, MAXREAD);
        n = read(connfd, buf, MAXREAD); // COMD UNAME PASS FNAME
        if ((int)n >= 0) {
            //printf("Request received\n");
            context = NULL;
            comd = strtok_r(buf, " \t\r\n\v\f", &context);
            if (strcmp(comd, "EXIT") == 0) {
                printf("EXIT called\n");
                exit(0);
            }
            uname = strtok_r(NULL, " \t\r\n\v\f", &context);
            pass = strtok_r(NULL, " \t\r\n\v\f", &context);
            //printf("%s %s\n", uname, pass);
            if (checkCreds(uname, pass)) {
                if (strcmp(comd, "GET") == 0) {
                    printf("GET called\n");
                    tgtpath = strtok_r(NULL, " \t\r\n\v\f", &context);
                    sprintf(fname,"./DFS%d/%s/%s", dfsno, uname, tgtpath);
                    fp = fopen(fname, "rb+");
                    if (fp == NULL) {
                        sprintf(resp, "FAILED TO OPEN FILE");
                        write(connfd, resp, strlen(resp));
                    } else {
                        n = fread(msg, 1, MAXREAD, fp);
                        if ((int)n >= 0) {
                            sprintf(resp, "SUCCESSFULLY READ FILE");
                            write(connfd, resp, strlen(resp));
                            bzero(buf1, MAXREAD);
                            m = read(connfd, buf1, MAXREAD);
                            if ((int)m >= 0) {
                                while ((int) n > 0) {
                                    printf("DATA SENT\n");
                                    write(connfd, msg, n);
                                    n = fread(msg, 1, MAXREAD, fp);
                                }
                            } else {
                                printf("NO RESPONSE RECEIVED\n");
                            }
                        } else {
                            sprintf(resp, "FAILED TO READ FILE");
                            write(connfd, resp, strlen(resp));
                        }
                        fclose(fp);
                    }
                } else if (strcmp(comd, "PUT") == 0) {
                    printf("PUT called\n");
                    tgtpath = strtok_r(NULL, " \t\r\n\v\f", &context);
                    //printf("%s\n", tgtpath);
                    keep = strtok_r(NULL, " \t\r\n\v\f", &context);
                    if (keep != NULL && strcmp(keep, "1") == 0)
                        keepopen = 1;
                    sprintf(fname,"./DFS%d/%s/%s", dfsno, uname, tgtpath);
                    sprintf(resp, "PUT RECEIVED");
                    write(connfd, resp, strlen(resp));
                    fp = fopen(fname, "wb+");
                    fclose(fp);
                    m = 1;
                    while ((int)m > 0) {
                        bzero(buf1, MAXREAD);
                        //printf("waiting for buf1\n");
                        m = read(connfd, buf1, MAXREAD); // DATA
                        //printf("buf1 = %s#\n", buf1);
                        if ((int)m > 0) {
                            fp = fopen(fname, "ab+");
                            fwrite(buf1, 1, m, fp);
                            fclose(fp);
                        }
                    }
                } else if (strcmp(comd, "LIST") == 0) {
                    printf("LIST called\n");
                    sprintf(dirname,"./DFS%d/%s", dfsno, uname);
                    fcount = 0;
                    d = opendir(dirname);
                    if (d != NULL) {
                        dir = readdir(d);
                        while(dir != NULL) {
                            if (dir != NULL && strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
                                fcount++;
                                strcpy(fname, dir->d_name);
                                bzero(temp1, 100);
                                memcpy(temp1, fname + 1, strlen(fname) - 3);
                                write(connfd, temp1, strlen(temp1));
                                m = read(connfd, buf1, MAXREAD);
                            }
                            dir = readdir(d);
                        }
                    }
                    if (fcount == 0) {
                        bzero(resp, MAXREAD);
                        sprintf(resp, "LIST EMPTY");
                        write(connfd, resp, strlen(resp));
                    }
                } else if (strcmp(comd, "MKDIR") == 0) {
                    printf("MKDIR called\n");
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
                printf("AUTH Failed\n");
                bzero(resp, MAXREAD);
                sprintf(resp, "AUTHENTICATION FAILED");
                write(connfd, resp, strlen(resp));
            }
        }
    }
    //printf("Closing thread\n");
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
    tv.tv_sec = 20;
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

