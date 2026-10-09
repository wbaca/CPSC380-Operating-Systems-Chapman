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

#define MAX_CLIENTS 10

// Define a struct for the clients 
typedef struct 
{
    int fd;
    int id;
    char buf[512 + 64];
    int buf_len;
} Client;
// Create array of clients to store each connected client
Client clients[MAX_CLIENTS];
// Variable to keep track of how many clients connect
int num_clients = 0;
// Initialize MUTual EXclusion device to keep threads from interfering 
// Mutex is useful for protecting shared data  
pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;


// Takes in a file descriptor and adds the client to the global clients array
void add_client(int client_fd)
{
    if(num_clients >= MAX_CLIENTS)
    {
        return;
    }
    else
    {
        pthread_mutex_lock(&client_mutex);
        clients[num_clients].fd = client_fd;
        clients[num_clients].id = num_clients + 1;
        clients[num_clients].buf_len = 0;
        num_clients++;  
        pthread_mutex_unlock(&client_mutex);
    }
}

// Takes in a file descriptor and removes the client from the global clients array
void remove_client(int client_fd)
{
    pthread_mutex_lock(&client_mutex);
    for (int i = 0; i < num_clients; i++)
    {
        if (clients[i].fd == client_fd)
        {
            for (int j = i; j < num_clients - 1; j++)
            {
                clients[j] = clients[j + 1];
            }
            num_clients--;
            break;
        }
    }
    pthread_mutex_unlock(&client_mutex);
}

// Takes in a file descriptor (int), a message string (char *), and a length of the string (size_t)
void broadcast(int sender_fd, char *message, size_t message_len)
{
    pthread_mutex_lock(&client_mutex);
    for (int i = 0; i < num_clients; i++)
    {
        if (clients[i].fd != sender_fd)
        {
            if (send(clients[i].fd, message, message_len, 0) == -1)
            {
                printf("Error sending message to client %d\n", clients[i].fd);
            }
        }
    }
    pthread_mutex_unlock(&client_mutex);
}

// Function passed to the thread 
// Receives and formats the message from a client
void *handle_client(void *arg)
{
    Client *client = (Client *)arg;
    ssize_t bytes_read;

    while (1)
    {
        // Using zero flag in recv() makes it operate like read()
        bytes_read = recv(client->fd, client->buf + client->buf_len, sizeof(client->buf) - client->buf_len, 0);
        // Check for errors and disconnections
        if (bytes_read == 0)
        {
            printf("Client %d disconnected\n", client->id);
            break;
        }
        else if (bytes_read == -1)
        {
            printf("Error receiving from socket\n");
            printf("Client %d lost connection\n", client->id);
            break;
        }
        // Check for message size 
        if (client->buf_len >= 512)
        {
            client->buf_len = 0;
        }
        client->buf_len += bytes_read;

        int start = 0;
        for(int i = 0; i < client->buf_len; i++)
        {
            if (client->buf[i] == '\n')
            {
                // Extract message
                int msg_len = i - start;
                if (msg_len <= 512)
                {
                    char message[512];
                    memcpy(message, client->buf + start, msg_len);
                    message[msg_len] = '\0';

                    // Format message with client id 
                    char formatted[512 + 64];
                    int formatted_len = snprintf(formatted, sizeof(formatted), "%d: %s\n", client->id, message);

                    // Send formatted version of the message
                    broadcast(client->fd, formatted, formatted_len);
                }
                else
                {
                    printf("Message too long from client %d, discarding it\n", client->id);
                    break;
                }
                // Shift remaining bytes down
                start = i + 1;
            }
        }
        // memmove(void dest[n], const void src[n], size_t n)
        // Copies n bytes to from src to dest 
        memmove(client->buf, client->buf + start, client->buf_len - start);
        client->buf_len -= start;
        if (start == 0 && client->buf_len > 512)
        {
            printf("Message too long from client %d, discarding\n", client->id);
            client->buf_len = 0;
        }
    }
    // Clean up
    remove_client(client->fd);
    close(client->fd);
    pthread_exit(0);
    return NULL;
}


int main(int argc, char *argv[])
{
    // Check command line input
    if (argc < 2)
    {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    
    // Create variables for the threads, server file descriptors, and server port
    pthread_t tid;
    int server_fd;
    int server_port = atoi(argv[1]);

    // Create a socket using TCP
    server_fd = socket(AF_INET, SOCK_STREAM, 0); 
    if (server_fd == -1)
    {
        printf("Error creating socket\n");
        exit(1);
    }

    // Initialize struct addr
    // Need length(memset), family, port, and address
    struct sockaddr_in addr;
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

        // Check for errors 
        if (client_fd == -1)
        {
            printf("Accept error\n");
            continue;
        }
        if (num_clients >= MAX_CLIENTS)
        {
            printf("Max clients reached, rejecting connection\n");
            close(client_fd);
            continue;
        }

        // Add the client using the file descriptor
        add_client(client_fd);
        printf("New connection from client %d\n", clients[num_clients - 1].id);

        // Create the thread for the client
        if (pthread_create(&tid, NULL, handle_client, &clients[num_clients - 1]) != 0)
        {
            printf("Error creating thread: %s\n", strerror(errno));;
            continue;
        }
        
        // Clean up
        pthread_detach(tid);
        pthread_mutex_destroy(&client_mutex);
    }
    return 0;
}