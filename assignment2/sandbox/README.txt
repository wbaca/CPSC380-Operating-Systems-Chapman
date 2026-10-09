To compile and run:
    > gcc chatserver.c -o chatserver
    > ./charserver <port number>


This server uses TCP to communicate between sockets. The user specifies a port number on the command line at execution. We make use of the netcat (nc) command to simulate the connection of multiple clients to the server.  Each new client that connects to the server gets its own thread to run on. All messages sent to any clients appear on all other clients. Each client gets a unique id number with a maximum of 10 clients running at once. I made use of pthread_mutex which I didn't need to have for the program to run but it separates, or locks, a thread to keep them from interfering with one another. 
