/* **********************************************

Will Baca
CPSC380
Assignment 1

***********************************************/

#include "mailbox-2.h"

#if __has_builtin(__builtin___memset_chk) || defined(__GNUC__)
#undef memset
/* void *memset(void *b, int c, size_t len) */
#define memset(dest, ...) \
		__builtin___memset_chk (dest, __VA_ARGS__, __darwin_obsz0 (dest))
#endif

void mailbox_init(mailbox_t *mbox)
{
    if (mbox == NULL)
    {
        return;
    }

    mbox->head = 0;
    mbox->tail = 0;

    memset(mbox->slots, 0, sizeof(mbox->slots));

}

int send_mailbox(mailbox_t *mbox, const void *msg, size_t len)
{
    if (mbox == NULL)
    {
        return MBOX_ERR_INVALID;
    } 
    else if (msg == NULL)
    {
        return MBOX_ERR_INVALID;
    }
    else if (len > MAILBOX_MAX_MSG)
    {
        return MBOX_ERR_TOO_LARGE;
    }
    else if ((mbox->tail - mbox->head) == MAILBOX_CAPACITY)
    {
        return MBOX_ERR_FULL;
    }
    else
    {
        // Create variable for the position
        int slot = mbox->tail % MAILBOX_CAPACITY;
        // Copy data from the message into the mailbox slot
        memcpy(mbox->slots[slot].data, msg, len);
        // Set the length of the message
        mbox->slots[slot].len = len;
        // Increment the tail for index of next slot to write to
        mbox->tail++;
        return MBOX_SUCCESS;
    }

}

int receive_mailbox(mailbox_t *mbox, void *out_buf, size_t maxlen, size_t *out_len)
{
    if (mbox->tail == mbox->head) 
    {
        return MBOX_ERR_EMPTY;
    } 
    else if (mbox == NULL || out_buf == NULL)
    {
        return MBOX_ERR_INVALID;
    }
    else
    {
        // Calculate the slot separate from index
        int slot = mbox->head % MAILBOX_CAPACITY;

        // Create variables to copy at most maxlen bytes to outbuf
        size_t msg_len = mbox->slots[mbox->head].len;
        size_t bytes_to_cp = msg_len;
        if (msg_len > maxlen) bytes_to_cp = maxlen;
        
        // Copy the bytes to outbuf
        memcpy(out_buf, mbox->slots[slot].data, bytes_to_cp);
        *out_len = msg_len;

        // Increment the head for index of next message to read
        mbox->head++;
        return MBOX_SUCCESS;
    }
}

// Prints the mailbox
void print_mailbox(mailbox_t *mbox)
{
    printf("\n===========  MAILBOX  ============\n");
    for(int i = 0; i < MAILBOX_CAPACITY; i++)
    {
        size_t msg_len = mbox->slots[i].len;
        printf("= Slot %d | ", i); 
        if (msg_len == 0)
        {
            printf("[empty]\n");
        }
        else
        {
            printf("[");
            for(size_t j = 0; j < msg_len; j++)
            {
                printf("%c", mbox->slots[i].data[j]);
            }
            printf("] =\n");
        }
    }
    printf("==================================\n\n");
}
