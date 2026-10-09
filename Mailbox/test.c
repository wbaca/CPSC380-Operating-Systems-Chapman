#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>

int main() {
    // 1. Ask the OS for ONE single shared integer
    int *shared_tail = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *shared_tail = 0;

    pid_t pid = fork();

    if (pid == 0) {
        // --- CHILD ---
        sleep(1); // Wait 1 second for the parent to do its job
        printf("CHILD: I woke up! The shared tail is: %d\n", *shared_tail);
        exit(0);
    } 
    else if (pid > 0) {
        // --- PARENT ---
        *shared_tail = 7; // Update the memory
        printf("PARENT: I changed the tail to %d. Now waiting for child...\n", *shared_tail);
        wait(NULL);
    }

    return 0;
}