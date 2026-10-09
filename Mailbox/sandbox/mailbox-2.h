#ifndef MAILBOX_H
#define MAILBOX_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>


/*
 * Asynchronous Shared-Memory Mailbox
 *
 * This mailbox supports indirect IPC between exactly:
 *      - One sender process
 *      - One receiver process
 *
 */

/* ---------------- Configuration Constants ---------------- */

#define MAILBOX_CAPACITY   8     /* maximum number of messages */
#define MAILBOX_MAX_MSG    64    /* maximum bytes per message */

/* ---------------- Return Codes ---------------- */

#define MBOX_SUCCESS        0
#define MBOX_ERR_INVALID   -1
#define MBOX_ERR_FULL      -2
#define MBOX_ERR_EMPTY     -3
#define MBOX_ERR_TOO_LARGE -4

/* ---------------- Message Slot ---------------- */

/*
 * Each slot holds one message.
 */

typedef struct {
    size_t len;                        /* length of valid data */
    uint8_t data[MAILBOX_MAX_MSG];     /* message contents */
} mailbox_slot_t;

/* ---------------- Mailbox Structure ---------------- */

/*
 * This structure must reside in shared memory.
 *
 * Students are responsible for correctly managing:
 *      - head index
 *      - tail index
 *      - FIFO behavior
 *
 * Only ONE process may call send().
 * Only ONE process may call receive().
 */
typedef struct {

    volatile int head;   /* index of next message to read */
    volatile int tail;   /* index of next slot to write */

    mailbox_slot_t slots[MAILBOX_CAPACITY];

} mailbox_t;

/* ---------------- API Functions ---------------- */

/*
 * Initialize a mailbox structure.
 * Must be called exactly once before fork().
 */    
void mailbox_init(mailbox_t *mbox);

/*
 * Asynchronous send.
 *
 * Returns:
 *   MBOX_SUCCESS on success
 *   MBOX_ERR_FULL if mailbox is full
 *   MBOX_ERR_TOO_LARGE if len > MAILBOX_MAX_MSG
 *   MBOX_ERR_INVALID on invalid arguments
 */
int send_mailbox(mailbox_t *mbox,
                 const void *msg,
                 size_t len);

/*
 * Asynchronous receive.
 *
 * Copies at most maxlen bytes into out_buf.
 *
 * On success:
 *   - returns MBOX_SUCCESS
 *   - writes actual message length to *out_len
 *
 * Returns:
 *   MBOX_ERR_EMPTY if mailbox is empty
 *   MBOX_ERR_INVALID on invalid arguments
 */
int receive_mailbox(mailbox_t *mbox,
                    void *out_buf,
                    size_t maxlen,
                    size_t *out_len);



void print_mailbox(mailbox_t *mbox);
void print_head(mailbox_t *mbox);
void print_tail(mailbox_t *mbox);

#endif /* MAILBOX_H */