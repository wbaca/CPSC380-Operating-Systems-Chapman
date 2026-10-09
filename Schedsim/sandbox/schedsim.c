/*
Your scheduler must be implemented in C or C++ and invoked as follows:
    ./schedsim <algorithm> <input_file> [time_quantum]
The <algorithm> argument selects the scheduling policy:
    • priority for preemptive priority scheduling
    • rr for round-robin scheduling
    • cfs for the simplified fair scheduler
The <input_file> describes the workload. The time_quantum parameter is required only when
using Round Robin.
For example:
    ./schedsim priority tasks.txt
    ./schedsim rr tasks.txt 4
    ./schedsim cfs tasks.txt
*/

/*
System Architecture
    Your system must consist of one scheduler thread and one thread per process. Each process
    thread represents a simulated process and does not run independently. Instead, it waits until the
    scheduler explicitly allows it to execute.

    Synchronization is achieved using semaphores. Each process has its own semaphore, and there is
    a global semaphore used by the scheduler to detect when a process has completed its assigned
    time unit.

    At every simulated time unit, the scheduler selects a process, signals it to run for exactly one tick
    (iteration), waits for it to finish, and then advances the system clock.

    input file format 
    PID Arrival Burst Priority
    P1  0       8     3
    P2  1       4     1
    P3  2       9     4
    P4  3       5     2
*/
#include "schedsim.h"


int main(int argc, char *argv[])
{
    char *algorithm = argv[1];
    char *input_file = argv[2];
    char *time_quantum = argv[3];
    Process processes[MAXPROCESSES];
    SchedulerArgs scheduler_args;
    //int quantum = 0;
    FILE *file;
    char line[256];
    char sem_name[32];
    pthread_t scheduler_thread = NULL;
    pthread_t process_threads[MAXPROCESSES];

    if (argc < 3)
    {
        fprintf(stderr, "Usage: %s <algorithm> <input_file> [time_quantum]\n", argv[0]);
        return EXIT_FAILURE;
    }
    else if(strcmp(algorithm, "rr") == 0 && argc < 4)
    {
        fprintf(stderr, "Round Robin scheduling requires a time quantum\n");
        return EXIT_FAILURE;
    }
    file = fopen(input_file, "r");
    if (!file)
    {
        perror("Error opening input file");
        return EXIT_FAILURE;
    }

    sem_unlink("/scheduler_sem");

    fgets(line, sizeof(line), file); // Skip header line
    while(fgets(line, sizeof(line), file) && task_count < MAXPROCESSES)
    {
        sscanf(line, "%s %d %d %d", processes[task_count].pid, 
                                    &processes[task_count].arrival_time, 
                                    &processes[task_count].burst_time, 
                                    &processes[task_count].priority);
        processes[task_count].remaining = processes[task_count].burst_time;
        processes[task_count].response_time = -1;
        processes[task_count].has_started = 0;
        // Initialize semaphore for each process
        snprintf(sem_name, sizeof(sem_name), "/sem_%s", processes[task_count].pid);

        sem_unlink(sem_name);
        processes[task_count].semaphore = sem_open(sem_name, O_CREAT, 0644, 0);
        task_count++;
    }

    for(int i = 0; i < task_count; i++)
    {
        pthread_create(&process_threads[i], NULL, process_routine, (void*)&processes[i]);
    }

    // for(int i = 0; i < task_count; i++)
    // {
    //     printf("Loaded Task: %s, Arrival: %d, Burst: %d, Priority: %d\n", processes[i].pid, processes[i].arrival_time, processes[i].burst_time, processes[i].priority);
    // }

    scheduler_args.processes = processes;
    scheduler_args.task_count = task_count;
    scheduler_args.quantum = (time_quantum) ? atoi(time_quantum) : 0;
    scheduler_args.workload_file = input_file;


    if (strcmp(algorithm, "rr") == 0)
    {
        pthread_create(&scheduler_thread, NULL, scheduler_rr, (void*)&scheduler_args);
    }
    else if (strcmp(algorithm, "priority") == 0)
    {
        pthread_create(&scheduler_thread, NULL, scheduler_priority, (void*)&scheduler_args);
    }
    else if (strcmp(algorithm, "cfs") == 0)
    {
        pthread_create(&scheduler_thread, NULL, scheduler_cfs, (void*)&scheduler_args);
    }
    else 
    {
        fprintf(stderr, "Unknown scheduling algorithm: %s\n", algorithm);
        return EXIT_FAILURE;
    }

    pthread_join(scheduler_thread, NULL);
    sem_close(scheduler_semaphore);
    sem_unlink("/scheduler_sem");

    for(int i = 0; i < task_count; i++)
    {
        pthread_join(process_threads[i], NULL);
        snprintf(sem_name, sizeof(sem_name), "/sem_%s", processes[i].pid);
        sem_close(processes[i].semaphore);
        sem_unlink(sem_name);
    }
    return EXIT_SUCCESS;
}
