#include "mailbox-2.h"

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
        int slot = mbox->tail % MAILBOX_CAPACITY;
        memcpy(mbox->slots[mbox->tail].data, msg, len);
        mbox->slots[mbox->tail].len = len;
        mbox->tail++;
        //mbox->tail = (mbox->tail + 1) % MAILBOX_CAPACITY;
        return MBOX_SUCCESS;
    }

}

int receive_mailbox(mailbox_t *mbox, void *out_buf, size_t maxlen, size_t *out_len)
{
    if (mbox->head == mbox->tail)
    {
        return MBOX_ERR_EMPTY;
    } 
    else if (mbox == NULL || out_buf == NULL)
    {
        return MBOX_ERR_INVALID;
    }
    else
    {
        size_t msg_len = mbox->slots[mbox->head].len;

        size_t bytes_to_cp = msg_len;
        if (msg_len > maxlen)
        {
            bytes_to_cp = maxlen;
        }
        
        memcpy(out_buf, mbox->slots[mbox->head].data, bytes_to_cp);

        *out_len = msg_len;

        mbox->head = (mbox->head + 1) % MAILBOX_CAPACITY;
        return MBOX_SUCCESS;
    }
}

void print_mailbox(mailbox_t *mbox)
{
    printf("\n===========  MAILBOX  ===========\n");
    for(int i = 0; i < MAILBOX_CAPACITY; i++)
    {
        size_t msg_len = mbox->slots[i].len;
        printf("= Slot %d | ", i); //| Len: %2zu | ", i, msg_len)
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

void print_head(mailbox_t *mbox)
{
    printf("Head: %d\n", mbox->head);
}
void print_tail(mailbox_t *mbox)
{
    printf("Tail: %d\n", mbox->tail);
}