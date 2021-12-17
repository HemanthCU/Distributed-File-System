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
#define MAXREAD  200000

int open_listenfd(int port);
int open_sendfd(int port, char *hostip);
void performOp(int op, char *fname, char *conffname);
int getHashVal(char* fname);

int main(int argc, char **argv) {
    int op, *connfdp, port, clientlen=sizeof(struct sockaddr_in);
    char* oper = (char*) malloc (100 * sizeof(char));
    char* fname = (char*) malloc (100 * sizeof(char));
    char* conffname = (char*) malloc (100 * sizeof(char));
    struct sockaddr_in clientaddr;
    pthread_t tid; 

    if (argc != 3) {
        fprintf(stderr, "usage: %s <port> <conf_file_name>\n", argv[0]);
        exit(0);
    }
    port = atoi(argv[1]);
    strcpy(conffname, argv[2]);
    printf("Operations available:\n");
    printf("1) get\n");
    printf("2) put\n");
    printf("3) list\n");
    printf("4) exit\n");
    while(1) {
        op = 0;
        printf("Enter the operation:\n");
        bzero(oper, 100);
        bzero(fname, 100);
        scanf("%s", oper);
        if (strcmp(oper, "get") == 0) {
            printf("Enter the filename:\n");
            scanf("%s", fname);
            op = 1;
        } else if (strcmp(oper, "put") == 0) {
            printf("Enter the filename:\n");
            scanf("%s", fname);
            op = 2;
        } else if (strcmp(oper, "list") == 0) {
            strcpy(fname, "");
            op = 3;
        } else if (strcmp(oper, "exit") == 0) {
            strcpy(fname, "");
            op = 4;
        } else {
            printf("\nInvalid operation\n");
            return 0;
        }
        if (op >= 1 && op <= 3)
            performOp(op, fname, conffname);
        else if (op == 4) {
            performOp(op, fname, conffname);
            return 0;
        }
    }
    return 0;
}

int getHashVal(char* fname) {
    return (int)strlen(fname) % 4;
}

void performOp(int op, char *fname, char *conffname) {
    int connfd;
    size_t n, m, l, k;
    int n1;
    int i, j, keepopen = 1;
    char buf[MAXREAD];
    char buf1[MAXREAD];
    char *resp = (char*) malloc (MAXREAD*sizeof(char));
    char *resp1 = (char*) malloc (MAXREAD*sizeof(char));
    char *msg = (char*) malloc (MAXREAD*sizeof(char));
    char **msgpart = (char**) malloc (4*sizeof(char*));
    char *confdata = (char*) malloc (MAXREAD*sizeof(char));
    char **dfsip = (char**) malloc (4*sizeof(char*));
    char **dfsport = (char**) malloc (4*sizeof(char*));
    for (i = 0; i < 4; i++) {
        msgpart[i] = (char*) malloc (MAXREAD*sizeof(char));
        dfsip[i] = (char*) malloc (20*sizeof(char));
        dfsport[i] = (char*) malloc (6*sizeof(char));
    }
    char *context = NULL;
    char *temp;
    char *tgtpath1 = (char*) malloc (100*sizeof(char));
    char *uname = (char*) malloc (100*sizeof(char));
    char *pass = (char*) malloc (100*sizeof(char));
    char *fname1 = (char*) malloc (100*sizeof(char));
    char **flist = (char**) malloc (100*sizeof(char*));
    int *ccount = (int*) malloc (100*sizeof(int));
    for (i = 0; i < 100; i++) {
        flist[i] = (char*) malloc (100*sizeof(char));
    }
    char c;
    FILE *fp, *fp1;
    int dfsfd1[4];
    int dfsfd2[4];
    int partsize[4];
    int sendport[4];
    int x = getHashVal(fname);
    int pnt;
    int fcount;
    int ffound;

    fp1 = fopen(conffname, "rb+");
    n = fread(confdata, 1, MAXREAD, fp1);
    fclose(fp1);
    //printf("%s\n", confdata);
    for (i = 0; i < 4; i++) {
        if (i == 0)
            temp = strtok_r(confdata, " \t\r\n\v\f:", &context);
        else
            temp = strtok_r(NULL, " \t\r\n\v\f:", &context);
        temp = strtok_r(NULL, " \t\r\n\v\f:", &context);
        temp = strtok_r(NULL, " \t\r\n\v\f:", &context);
        strcpy(dfsip[i], temp);
        temp = strtok_r(NULL, " \t\r\n\v\f:", &context);
        strcpy(dfsport[i], temp);
        sendport[i] = atoi(dfsport[i]);
    }

    temp = strtok_r(NULL, " \t\r\n\v\f:", &context);
    temp = strtok_r(NULL, " \t\r\n\v\f:", &context);
    strcpy(uname, temp);
    temp = strtok_r(NULL, " \t\r\n\v\f:", &context);
    temp = strtok_r(NULL, " \t\r\n\v\f:", &context);
    strcpy(pass, temp);

    if (op == 1) {
        //GET
        for (i = 0; i < 4; i++) {
            dfsfd1[(i + x) % 4] = open_sendfd(sendport[(i + x) % 4], dfsip[(i + x) % 4]);
            sprintf(fname1, ".%s.%d", fname, i + 1);
            sprintf(resp, "GET %s %s %s end", uname, pass, fname1);
            write(dfsfd1[(i + x) % 4], resp, strlen(resp));
            bzero(buf, MAXREAD);
            n = read(dfsfd1[(i + x) % 4], buf, MAXREAD);
            if ((int)n < 0 || buf[0] == 'F') {
                printf("Server failed to read the file\n");
                return;
            } else if (buf[0] == 'A') {
                printf("Authentication Failed\n");
                return;
            }
            sprintf(resp, "READY TO RECEIVE");
            write(dfsfd1[(i + x) % 4], resp, strlen(resp));
            bzero(buf, MAXREAD);
            pnt = 0;
            while((int)n > 0) {
                n = read(dfsfd1[(i + x) % 4], buf, MAXREAD);
                memcpy(buf1 + pnt, buf, n);
                pnt += (int)n;
            }
            bzero(msgpart[i], MAXREAD);
            memcpy(msgpart[i], buf1, pnt);
            partsize[i] = pnt;
        }
        fp = fopen(fname, "wb+");
        for (i = 0; i < 4; i++) {
            fwrite(msgpart[i], 1, partsize[i], fp);
        }
        fclose(fp);
        for (i = 0; i < 4; i++) {
            close(dfsfd1[i]);
        }
    } else if (op == 2) {
        //PUT
        fp = fopen(fname, "rb+");
        n = fread(msg, 1, MAXREAD, fp);
        fclose(fp);
        n1 = (int) n;
        //printf("n1 = %d\n", n1);
        for (i = 0; i < 4; i++) {
            partsize[i] = (i == 3) ? n1 - (3 * (n1 / 4)) : n1 / 4;
            //SPLIT INTO PARTS
            memcpy(msgpart[i], msg + (i * (n1 / 4)), partsize[i]);
        }

        //SEND MKDIR TO SERVERS
        bzero(resp, MAXREAD);
        sprintf(resp, "MKDIR %s %s", uname, pass);
        //printf("%s\n", resp);
        for (i = 0; i < 4; i++) {
            dfsfd1[i] = open_sendfd(sendport[i], dfsip[i]);
            write(dfsfd1[i], resp, strlen(resp));
            bzero(buf, MAXREAD);
            n = read(dfsfd1[i], buf, MAXREAD);
            if ((int)n < 0 || buf[0] == 'F') {
                printf("Failed to create directories\n");
                return;
            } else if (buf[0] == 'A') {
                printf("Authentication Failed\n");
                return;
            }
        }

        //SEND PUT AND DATA TO SERVERS
        for (i = 0; i < 4; i++) {
            dfsfd1[(i + x) % 4] = open_sendfd(sendport[(i + x) % 4], dfsip[(i + x) % 4]);
            bzero(fname1, 100);
            sprintf(fname1, ".%s.%d", fname, i + 1);
            //printf("%s\n", fname1);
            bzero(resp, 100);
            sprintf(resp, "PUT %s %s %s end", uname, pass, fname1);
            write(dfsfd1[(i + x) % 4], resp, strlen(resp));
            l = read(dfsfd1[(i + x) % 4], buf, MAXREAD);
            if ((int)l < 0)
                return;
            write(dfsfd1[(i + x) % 4], msgpart[i], partsize[i]);

            dfsfd2[(i + x) % 4] = open_sendfd(sendport[(i + x) % 4], dfsip[(i + x) % 4]);
            bzero(fname1, 100);
            sprintf(fname1, ".%s.%d", fname, ((i + 1) % 4) + 1);
            //printf("%s\n", fname1);
            bzero(resp, 100);
            sprintf(resp, "PUT %s %s %s end", uname, pass, fname1);
            write(dfsfd2[(i + x) % 4], resp, strlen(resp));
            k = read(dfsfd2[(i + x) % 4], buf, MAXREAD);
            if ((int)k < 0)
                return;
            write(dfsfd2[(i + x) % 4], msgpart[(i + 1) % 4], partsize[(i + 1) % 4]);
            //printf("\n");
        }
        for (i = 0; i < 4; i++) {
            close(dfsfd1[i]);
            close(dfsfd2[i]);
        }
    } else if (op == 3) {
        //LIST
        fcount = 0;
        sprintf(resp, "LIST %s %s", uname, pass);
        for (i = 0; i < 4; i++) {
            dfsfd1[i] = open_sendfd(sendport[i], dfsip[i]);
            write(dfsfd1[i], resp, strlen(resp));
            n = 1;
            while ((int) n > 0) {
                bzero(buf, MAXREAD);
                n = read(dfsfd1[i], buf, MAXREAD);
                if ((int)n < 0 || strcmp(buf, "AUTHENTICATION FAILED") == 0) {
                    printf("Authentication Failed\n");
                    return;
                } else if (strcmp(buf, "LIST EMPTY") == 0) {
                    printf("No files available\n");
                    return;
                }
                sprintf(resp1, "ITEM RECEIVED");
                write(dfsfd1[i], resp1, strlen(resp1));
                if ((int) n > 0) {
                    ffound = 0;
                    for (j = 0; j < fcount; j++) {
                        if (strcmp(buf, flist[j]) == 0) {
                            ccount[j]++;
                            ffound = 1;
                        }
                    }
                    if (ffound == 0) {
                        ccount[fcount] = 1;
                        bzero(flist[fcount], 100);
                        strcpy(flist[fcount], buf);
                        fcount++;
                    }
                }
            }
        }
        printf("\nThe following files are available:\n");
        for (i = 0; i < fcount; i++) {
            printf("%s", flist[i]);
            if (ccount[i] != 8) {
                printf(" [Incomplete]");
            }
            printf("\n");
        }
        printf("\n");
        for (i = 0; i < 4; i++) {
            close(dfsfd1[i]);
        }
    } else if (op == 4) {
        bzero(resp, MAXREAD);
        sprintf(resp, "EXIT");
        for (i = 0; i < 4; i++) {
            dfsfd1[i] = open_sendfd(sendport[i], dfsip[i]);
            write(dfsfd1[i], resp, strlen(resp));
        }
    }
    //printf("Closing thread\n");
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

