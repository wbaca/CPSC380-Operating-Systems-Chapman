/* **********************************************

Will Baca
CPSC380
Assignment 1

***********************************************/
#include "mailbox-2.h"

int main(int argc, char *argv[])
{
    // Allocate variables 
    const char *shared_memory_name = "/my_shared_mailbox_memory";
    int shm_fd, status_rcv, empty_stat, status_snd, j; 
    pid_t pid;
    int num_messages = 0;
    char outbuf[MAILBOX_MAX_MSG];
    size_t actual_len; // Used for the bytes written to the outbuf in receive_message()
    mailbox_t *mailbox;

    // Check user input 
    if (argc < 3)
    {
        printf("Usage: ./mboxtext.exe <number of messages to send> <message> <message> ... (max of 8, 64 letter, messages) \n");
        exit(1);
    }
    else if (atoi(argv[1]) != argc - 2)
    {
        printf("Number of messages specified differs from amount of messages given...\n");
        exit(1);
    }

    // Take number of messages from command line and convert to integer 
    num_messages = atoi(argv[1]);

    shm_unlink(shared_memory_name);

    // Open a shared space in memory for the mailbox
    shm_fd = shm_open(shared_memory_name, O_CREAT | O_RDWR | O_TRUNC, 0666);

    // Check for errors
    if (shm_fd == -1)
    {
        perror("shm_open failed");
        exit(EXIT_FAILURE);
    }

    // Truncate the memory to the size of the mailbox
    if (ftruncate(shm_fd, sizeof(mailbox_t)) == -1)
    {
        perror("ftruncate failed");
        exit(EXIT_FAILURE);
    }

    // Create the mailbox and map it to the shared memory space
    mailbox = mmap(NULL, sizeof(mailbox_t), 
                            PROT_READ | PROT_WRITE, 
                            MAP_SHARED | O_TRUNC, shm_fd, 0);

    // Check for errors
    if (mailbox == MAP_FAILED)
    {
        perror("map failed\n");
        exit(EXIT_FAILURE);
    }

    // Initialize mailbox
    mailbox_init(mailbox);
    // Show empty mailbox
    print_mailbox(mailbox);

    // Create the parent and child process
    pid = fork();

    // Check to make sure fork() worked
    if (pid < 0)
    {
        perror("fork failed\n");
        exit(EXIT_FAILURE);
    }
    else if (pid == 0)
    {
        // Child process
        sleep(1); // Wait for parent to finish sending messages 
        printf("Child starting receiver\n\n"); 
        // Loop through the messages to receive them 
        for(int i = 0; i < num_messages; i++)
        {   
            if (i >= MAILBOX_CAPACITY)
            {
                break;
            }
            status_rcv = receive_mailbox(mailbox, outbuf, MAILBOX_MAX_MSG, &actual_len);
            if (status_rcv == MBOX_SUCCESS)
            {
                // Initialize the outbuffer to hold a message
                outbuf[actual_len] = '\0';
                printf("Child received: %s\n", outbuf);
            } 
            else if (status_rcv == MBOX_ERR_EMPTY)
            {
                printf("\nChild detected empty mailbox\n");
                exit(EXIT_FAILURE);
            }
        }
        // Check for empty mailbox if no message is sent
        empty_stat = receive_mailbox(mailbox, outbuf, MAILBOX_MAX_MSG, &actual_len);
        if (empty_stat == MBOX_ERR_EMPTY)
        {
            printf("Child detected empty mailbox\n");
        }
        exit(EXIT_SUCCESS);
    } 
    else if (pid > 0)
    {
        // Parent process
        printf("Parent start sender\n");
    
        j = 2;
        for(int i = 0; i < num_messages; i++)
        {
            char msg[64];
            sprintf(msg, "%s", argv[j++]); 
            status_snd = send_mailbox(mailbox, msg, strlen(msg));

            if (status_snd == MBOX_SUCCESS)
            {
                printf("Parent sent: %s\n", msg);
            } 
            else if (status_snd == MBOX_ERR_FULL)
            {
                printf("Parent detected full mailbox\n");
                break;
            } 
            else if (status_snd == MBOX_ERR_TOO_LARGE)
            {
                printf("Message too large\n");
                exit(EXIT_FAILURE);
            }
            else if (status_snd == MBOX_ERR_INVALID)
            {
                printf("Invalid arguments\n");
                exit(EXIT_FAILURE);
            }
        }
        print_mailbox(mailbox);
        wait(NULL);
        printf("\nParent: child finished\n");
    }

    munmap(mailbox, sizeof(mailbox_t));

    shm_unlink(shared_memory_name);

    return 0;
}
