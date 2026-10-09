#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

// create a thread after accept() is called 
/*
int socket(int domain, int type, int protocol);
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr *_Nullable restrict addr, socklen_t *_Nullable restrict addrlen);


ssize_t recv(int sockfd, void buf[size], size_t size, int flags);
ssize_t send(int sockfd, const void buf[size], size_t size, int flags);

*/

/*
The server will create a socket with the socket() function, then it has to bind the server to the socket using the bind() function. 
Then it has to start listening on the socket using the listen() function. 
The server then begins listening to the port with the accept() function. 
The server will then block on the accept() call, so it is after this point where you have to create a thread to handle each client connection. 
The server thread then reads (recv) the message from the client and writes (send) to all the other connected clients.

*/

#define MAX_CLIENTS 10
void add_client(int client_fd);
void remove_client(int client_fd);
void broadcast(int sender_fd, char *message, size_t message_len);
void *handle_client(void *arg);

static int client_fds[MAX_CLIENTS];
static int num_clients = 0;
// Initialize mutex to keep track of connections and disconnections 
static pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;

// Takes in an integer and adds a client file descriptor to the client_fds array
void add_client(int client_fd)
{
        pthread_mutex_lock(&client_mutex);
        client_fds[num_clients++] = client_fd;
        pthread_mutex_unlock(&client_mutex);
}

void remove_client(int client_fd)
{
    pthread_mutex_lock(&client_mutex);
    for (int i = 0; i < num_clients; i++)
    {
        if (client_fds[i] == client_fd)
        {
            for (int j = i; j < num_clients - 1; j++)
            {
                client_fds[j] = client_fds[j + 1];
            }
            num_clients--;
            break;
        }
    }
    pthread_mutex_unlock(&client_mutex);
}
void broadcast(int sender_fd, char *message, size_t message_len)
{
    pthread_mutex_lock(&client_mutex);
    for (int i = 0; i < num_clients; i++)
    {
        if (client_fds[i] != sender_fd)
        {
            if (send(client_fds[i], message, message_len, 0) == -1)
            {
                printf("Error sending message to client %d\n", client_fds[i]);
            }
        }
    }
    pthread_mutex_unlock(&client_mutex);
}

void *handle_client(void *arg)
{
    char buffer[1024];
    int client_fd;
    ssize_t bytes_read;
    client_fd = *((int *)arg); 
    free(arg);

    while (1)
    {
        bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);
        if (bytes_read == 0)
        {
            printf("Client on fd %d disconnected\n", client_fd);
            break;
        }
        else if (bytes_read == -1)
        {
            printf("Error receiving from socket\n");
            printf("Client on fd %d lost connection\n", client_fd);
            break;
        }
        buffer[bytes_read] = '\0';
        printf("Received message: %s\n", buffer);
        broadcast(client_fd, buffer, (size_t)bytes_read);
    }

    remove_client(client_fd);
    close(client_fd);
    return NULL;
}


// og works 
// void *handle_client(void *arg)
// {
//     int client_fd = *((int *)arg);
//     free(arg);
//     char buffer[1024];
//     ssize_t bytes_read;

//     while (1)
//     {
//         bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);
//         if (bytes_read == 0)
//         {
//             printf("Client on fd %d disconnected\n", client_fd);
//             break;
//         }
//         else if (bytes_read == -1)
//         {
//             printf("Error receiving from socket\n");
//             printf("Client on fd %d lost connection\n", client_fd);
//             break;
//         }
//         buffer[bytes_read] = '\0';
//         printf("Received message: %s\n", buffer);
//         send(client_fd, buffer, bytes_read, 0);
//     }

//     close(client_fd);
//     return NULL;
// }


int main(int argc, char *argv[])
{
    void *fd_ptr;
    pthread_t tid;
    int server_fd, server_port;
    struct sockaddr_in addr;
    // Check command line input
    if (argc < 2)
    {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    
    // Create variables for the threads, server file descriptors, and server port
    server_port = atoi(argv[1]);

    // Create a socket using TCP
    server_fd = socket(AF_INET, SOCK_STREAM, 0); 
    if (server_fd == -1)
    {
        printf("Error creating socket\n");
        exit(1);
    }

    // Initialize variable addr
    // Need length(memset), family, port, and address
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(server_port);

    // Bind the created socket to the address
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        printf("Error binding socket\n");
        exit(1);
    }
    // Listen for connections 
    if (listen(server_fd, 5) == -1)
    {
        printf("Error listening on socket: %s\n", strerror(errno));
        exit(1);
    }
    printf("Listening on port %d\n", server_port);

    // Keep the connection open (persistent)
    while (1)
    {
        // Initialize client variables 
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        // Accept the client connection
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addrlen);

        if (client_fd == -1)
        {
            printf("Accept error\n");
            continue;
        }

        printf("New connection from fd %d\n", client_fd);

        // Add the client using the file descriptor
        
        add_client(client_fd);
        
        // Heap allocate the file descriptor each thread gets its own copy
        fd_ptr = malloc(sizeof(int));
        if (fd_ptr == NULL)
        {
            printf("Error allocating memory\n");
            close(client_fd);
            continue;
        }

        // Cast the client to a void pointer file descriptor to create the thread for that client 
        *(int *)fd_ptr = client_fd;

        // Create the thread for the client
        if (pthread_create(&tid, NULL, handle_client, fd_ptr) != 0)
        {
            printf("Error creating thread: %s\n", strerror(errno));
            free(fd_ptr);
            close(client_fd);
            continue;
        }
        
        // Clean up
        pthread_detach(tid);

        printf("[");
        for(int i = 0; i < MAX_CLIENTS; ++i)
        {
            printf("%d,",client_fds[i]);
        }
        printf("]");
    }

    //return 0;
}
