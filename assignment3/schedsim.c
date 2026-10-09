#include "schedsim.h"

int main(int argc, char *argv[])
{
    char *algorithm = argv[1];
    char *input_file = argv[2];
    char *time_quantum = argv[3];
    Process processes[MAXPROCESSES];
    SchedulerArgs scheduler_args;
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
    // Initialize scheduler semaphore
    sem_unlink("/scheduler_sem");
    scheduler_semaphore = sem_open("/scheduler_sem", O_CREAT, 0644, 0);
    if (scheduler_semaphore == SEM_FAILED) {
        perror("Error creating scheduler semaphore");
        fclose(file);
        return EXIT_FAILURE;
    }
    // Read processes from input file
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
    // Create threads for each process
    for(int i = 0; i < task_count; i++)
    {
        pthread_create(&process_threads[i], NULL, process_routine, (void*)&processes[i]);
    }
    // Initialize scheduler arguments
    scheduler_args.processes = processes;
    scheduler_args.task_count = task_count;
    scheduler_args.quantum = (time_quantum) ? atoi(time_quantum) : 0;
    scheduler_args.workload_file = input_file;
    // Create scheduler thread based on selected algorithm
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
    // Wait for scheduler thread to finish
    pthread_join(scheduler_thread, NULL);
    sem_close(scheduler_semaphore);
    sem_unlink("/scheduler_sem");
    // Wait for process threads to finish and clean up semaphores
    for(int i = 0; i < task_count; i++)
    {
        pthread_join(process_threads[i], NULL);
        snprintf(sem_name, sizeof(sem_name), "/sem_%s", processes[i].pid);
        sem_close(processes[i].semaphore);
        sem_unlink(sem_name);
    }
    return EXIT_SUCCESS;
}
