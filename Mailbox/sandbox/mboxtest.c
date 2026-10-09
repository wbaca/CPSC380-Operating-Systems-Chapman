/* **********************************************

    1. Creates the shared mailbox
    2. Calls fork()
    3. One process acts as sender
    4. One process acts as receiver
    5. Demonstrates:
        • Successful message exchange
        • Detection of full mailbox
        • Detection of empty mailbox
Your demo should clearly show message ordering.

***********************************************/
#include "mailbox-2.h"

int main()
{
    const char *shared_memory_name = "/my_shared_mailbox_memory";
    int shm_fd; 
    pid_t pid;
    char outbuf[MAILBOX_MAX_MSG];
    char msg[MAILBOX_MAX_MSG], msg1[MAILBOX_MAX_MSG], msg2[MAILBOX_MAX_MSG],
        msg3[MAILBOX_MAX_MSG], msg4[MAILBOX_MAX_MSG], msg5[MAILBOX_MAX_MSG],
        msg6[MAILBOX_MAX_MSG], msg7[MAILBOX_MAX_MSG], msg8[MAILBOX_MAX_MSG];
    size_t actual_len;

    shm_unlink(shared_memory_name);

    shm_fd = shm_open(shared_memory_name, O_CREAT | O_RDWR | O_TRUNC, 0666);

    if (shm_fd == -1)
    {
        perror("shm_open failed");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(shm_fd, sizeof(mailbox_t)) == -1)
    {
        perror("ftruncate failed");
        exit(EXIT_FAILURE);
    }

    mailbox_t *mailbox = mmap(NULL, sizeof(mailbox_t), 
                            PROT_READ | PROT_WRITE, 
                            MAP_SHARED | O_TRUNC, shm_fd, 0);

    if (mailbox == MAP_FAILED)
    {
        perror("map failed\n");
        exit(EXIT_FAILURE);
    }

    mailbox_init(mailbox);

    printf("MAIN Address: %p\n", (void*)mailbox);

    print_mailbox(mailbox);
    pid = fork();

    if (pid < 0)
    {
        perror("fork failed\n");
        exit(EXIT_FAILURE);
    }
    else if (pid == 0)
    {
        // child process
        // receiver
        sleep(1);
        //printf("CHILD Address: %p | Tail is: %d\n", (void*)mailbox, mailbox->tail);

        //printf("head: %d, tail: %d\n", mailbox->head, mailbox->tail);

        //print_head(mailbox);
        //print_tail(mailbox);
        printf("Child starting receiver\n\n"); 
        for(int i = 0; i < MAILBOX_CAPACITY; i++)
        {
            int status = receive_mailbox(mailbox, outbuf, MAILBOX_MAX_MSG, &actual_len);
            //printf("outbuf: %zu\n", actual_len);
            //printf("status: %d\n", status);
            if (status == MBOX_SUCCESS)
            {
                outbuf[actual_len] = '\0';
                printf("Child received: %s\n", outbuf);
            } 
            else if (status == MBOX_ERR_EMPTY)
            {
                printf("\nChild detected empty mailbox\n");
                exit(EXIT_FAILURE);
            }
        }
        //print_mailbox(mailbox);
        exit(EXIT_SUCCESS);
        //return 0;
    } 
    else if (pid > 0)
    {
        //printf("head: %d, tail: %d\n", mailbox->head, mailbox->tail);
        // parent process
        // sender
        printf("Parent start sender\n");
        //print_mailbox(mailbox);
        sprintf(msg, "First test message to send to the mailbox.");
        sprintf(msg1, "Second test message to send to the mailbox.");
        sprintf(msg2, "A third message to be sent to the mailbox with sixty four chars");
        sprintf(msg3, "A");
        sprintf(msg4, "");
        sprintf(msg5, "Another test message");
        sprintf(msg6, "Yet another test message");
        sprintf(msg7, "You guessed it another test message");
        sprintf(msg8, "Overflow message test");

        if (send_mailbox(mailbox, msg, strlen(msg)) == MBOX_SUCCESS) printf("Parent sent: %s\n", msg);
        if (send_mailbox(mailbox, msg1, strlen(msg1)) == MBOX_SUCCESS) printf("Parent sent: %s\n", msg1);
        if (send_mailbox(mailbox, msg2, strlen(msg2)) == MBOX_SUCCESS) printf("Parent sent: %s\n", msg2);
        if (send_mailbox(mailbox, msg3, strlen(msg3)) == MBOX_SUCCESS) printf("Parent sent: %s\n", msg3);
        if (send_mailbox(mailbox, msg4, strlen(msg4)) == MBOX_SUCCESS) printf("Parent sent: %s\n", msg4);
        if (send_mailbox(mailbox, msg5, strlen(msg5)) == MBOX_SUCCESS) printf("Parent sent: %s\n", msg5);
        if (send_mailbox(mailbox, msg6, strlen(msg6)) == MBOX_SUCCESS) printf("Parent sent: %s\n", msg6);
        if (send_mailbox(mailbox, msg7, strlen(msg7)) == MBOX_SUCCESS) printf("Parent sent: %s\n", msg7);
        print_mailbox(mailbox);
        //print_head(mailbox);
        //print_tail(mailbox);
        int status = send_mailbox(mailbox, msg8, strlen(msg8));
        if (status == MBOX_SUCCESS)
            {
                printf("Parent sent: %s\n", msg8);
               // printf("PARENT Address: %p | Tail is: %d\n", (void*)mailbox, mailbox->tail);
                //print_mailbox(mailbox);

            } 
            else if (status == MBOX_ERR_FULL)
            {
                printf("Parent detected full mailbox\n");
                //exit(EXIT_FAILURE);
            } 
            else if (status == MBOX_ERR_TOO_LARGE)
            {
                printf("Message too large\n");
                exit(EXIT_FAILURE);
            }
            else if (status == MBOX_ERR_INVALID)
            {
                printf("Invalid arguments\n");
                exit(EXIT_FAILURE);
            }
        // for(int i = 0; i < MAILBOX_CAPACITY; i++)
        // {
        //     char msg[64];
        //     sprintf(msg, "Message %d", i);
        //     int status = send_mailbox(mailbox, msg, strlen(msg));
           

        //     if (status == MBOX_SUCCESS)
        //     {
        //         printf("Parent sent: %s\n", msg);
        //        // printf("PARENT Address: %p | Tail is: %d\n", (void*)mailbox, mailbox->tail);
        //         //print_mailbox(mailbox);

        //     } 
        //     else if (status == MBOX_ERR_FULL)
        //     {
        //         printf("Parent detected full mailbox\n");
        //         //exit(EXIT_FAILURE);
        //     } 
        //     else if (status == MBOX_ERR_TOO_LARGE)
        //     {
        //         printf("Message too large\n");
        //         exit(EXIT_FAILURE);
        //     }
        //     else if (status == MBOX_ERR_INVALID)
        //     {
        //         printf("Invalid arguments\n");
        //         exit(EXIT_FAILURE);
        //     }
        // }
        // int full_status = send_mailbox(mailbox, "Overflow", 9);
        // if (full_status == MBOX_ERR_FULL)
        // {
        //     printf("Parent detected full mailbox\n");
        // }
        //printf("head: %d, tail: %d\n", mailbox->head, mailbox->tail);
        wait(NULL);
        printf("\nParent: child finished\n");
        //return 0;
    }

    //print_mailbox(mailbox);

    munmap(mailbox, sizeof(mailbox_t));

    shm_unlink(shared_memory_name);

    return 0;
}