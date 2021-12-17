# Network Systems Programming Assignment 4

This is an assignment to maintain a distributed file system that will split up a file into multiple parts and store these parts into 4 separate servers. The 4 parts are split up and 2 parts are sent each to each of the 4 servers creating a redundancy by making 2 copies of each part.

The program also authenticates the user based on a dfc.conf file and comapres it with the usernames and passwords in the dfs.conf file.

To run the program:

Build and run 4 instances of DFS.c as follows (Prefer to run them without the & in 4 separate terminals):

```
gcc -pthread DFS.c -o server -lssl -lcrypto
./server 10001 &
./server 10002 &
./server 10003 &
./server 10004 &
```

Build and run the client with the following:

```
gcc -pthread DFC.c -o client -lssl -lcrypto
./cleint 8000 dfc.conf
```

You can use the following 4 commands to perform the respective tasks (For get and put, the program will prompt for the filename after you enter the command):
get - Get the parts and combine it into a single file
put - Split the file and put it onto the servers
list - List the files available on the servers (Will mention if the file is incomplete)
exit - Terminate all servers and client
