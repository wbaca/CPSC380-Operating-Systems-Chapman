To compile and run:
    gcc mboxtest.c mailbox-2.c -o mboxtest.exe
    ./mboxtest.exe <number of messages> <message 1> <message 2> ...

I started with creating the mailbox_init() function and opening a shared space in memory for the mailbox messages.

To detect a full mailbox I first used a formula that used modulo to wrap around but I was running into issues because 
I had this written, (mbox->tail + 1) % MAILBOX_CAPACITY, in the else if statement. This is also how I was iterating through the mailbox which 
gave me issues because when it would get to mbox->tail = 7 it would miss the last message trying to be sent. I has to just increment the tail
separate from putting the message into the mailbox by making an int variable that would do the modulo separate from the increment. I changed the 
if else statement to, (mbox->tail - mbox->head) == MAILBOX_CAPACITY, which separates the increment from the check so all messages are included. 

To detect an empty mailbox I checked if, mbox->tail == mbox->head. This indicates that the program is looking at same slot in the mailbox.
Or when the head catches up to the tail. 

There is FIFO structure in that the parent sends all the messages then the child receives them in the same order they are sent. 