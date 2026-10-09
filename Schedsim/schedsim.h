#ifndef SCHEDSIM_H
#define SCHEDSIM_H

#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <string.h>

#define MAXPROCESSES 64

typedef struct 
{
    char pid[16];
    int arrival_time;
    int burst_time;
    int priority;
    int remaining;
    int wait_time;
    int turnaround_time;
    int response_time;
    int has_started;
    sem_t *semaphore; // semaphore for process synchronization
} Process;
typedef struct
{
    Process *processes;
    int task_count;
    int quantum;
    const char *workload_file;
} SchedulerArgs;

static sem_t *scheduler_semaphore; // semaphore for scheduler synchronization
static int task_count = 0;

void *process_routine(void *arg);
void *scheduler_rr(void *arg);
void *scheduler_priority(void *arg);
void *scheduler_cfs(void *arg);


void *process_routine(void *arg)
{
    Process *proc = (Process *)arg;
    while (proc->remaining > 0) {
        // Wait for the scheduler to signal this process to run
        sem_wait(proc->semaphore);
        // Simulate running for one time unit
        proc->remaining--;
        // Signal the scheduler that this process has completed its time unit
        sem_post(scheduler_semaphore);
    }
    pthread_exit(0);
}

void *scheduler_rr(void *arg)
{
    SchedulerArgs *args = (SchedulerArgs *)arg; // SchedulerArgs contains the list of processes, task count, and other relevant info
    int quantum = args->quantum; // Time quantum for Round Robin scheduling
    int current_time = 0; // Keep track of the current time in the simulation
    int proc_index = 0; // Index of the currently selected process to run
    int completed_tasks = 0; // Count of completed tasks to know when to stop the scheduler
    int context_switches = 0; // Count of context switches for metrics
    int prev_proc_index = -1; // Track the previously running process index to count context switches
    int total_ticks = 0; // Total time ticks for Gantt chart
    int ticks_run = 0; // Count of ticks the current process has run in its current time slice
    int gantt_history[1000]; // Store the order of process execution for Gantt chart
    int ready_queue[MAXPROCESSES]; // Track which processes are currently in the ready queue
    int in_queue[MAXPROCESSES] = {0}; // Track which processes are currently in the ready queue
    int head = 0, tail = 0, q_size = 0; // Variables for managing the ready queue as a circular buffer
    float total_waiting = 0, total_turnaround = 0, total_response = 0; // Metrics accumulators
    Process *proc; // Pointer to the currently running process
    FILE *log_file; // File pointer for logging results

    log_file = fopen("rr_results.csv", "a");
    if (!log_file) {
        perror("Error opening log file");
        pthread_exit(0);
    }

    while(completed_tasks < args->task_count)
    {
        for(int i = 0; i < args->task_count; i++)
        {
            // Check for newly arrived processes and add them to the ready queue
            if(args->processes[i].arrival_time <= current_time && !in_queue[i] && args->processes[i].remaining > 0)
            {
                ready_queue[tail] = i;
                tail = (tail + 1) % MAXPROCESSES;
                q_size++;
                in_queue[i] = 1;
            }
        }
        // If queue is empty CPU is idle advance time
        if(q_size == 0)
        {
            gantt_history[current_time] = -1; // Indicate idle time in Gantt chart
            current_time++;
            total_ticks = current_time;
            continue;
        }
        // Dequeue next process to run
        proc_index = ready_queue[head];
        head = (head + 1) % MAXPROCESSES;
        q_size--;
        proc = &args->processes[proc_index];

        if(prev_proc_index != proc_index)
        {
            context_switches++;
        }
        prev_proc_index = proc_index;
        if(proc->has_started == 0)
        {
            proc->response_time = current_time - proc->arrival_time;
            proc->has_started = 1;
        }

        // Signal the process to run for one time unit
        ticks_run = 0;
        while(ticks_run < quantum && args->processes[proc_index].remaining > 0)
        {
            // Record the process running in the Gantt chart
            gantt_history[current_time] = proc_index;
            total_ticks = current_time + 1;
            // Handshake with process thread
            sem_post(proc->semaphore);
            sem_wait(scheduler_semaphore);
            // Advance time and check for new arrivals during the time slice
            ticks_run++;
            current_time++;
            for(int i = 0; i < args->task_count; i++)
            {
                // Check for newly arrived processes and add them to the ready queue
                if(args->processes[i].arrival_time <= current_time && !in_queue[i] && args->processes[i].remaining > 0)
                {
                    ready_queue[tail] = i;
                    tail = (tail + 1) % MAXPROCESSES;
                    q_size++;
                    in_queue[i] = 1;
                }
            }
        }
        // If process still has remaining time, add it back to the end of the ready queue
        if (proc->remaining > 0)
        {
            ready_queue[tail] = proc_index;
            tail = (tail + 1) % MAXPROCESSES;
            q_size++;
        }
        else
        {
            completed_tasks++;
            in_queue[proc_index] = 0;

            proc->turnaround_time = current_time - proc->arrival_time;
            proc->wait_time = proc->turnaround_time - proc->burst_time;

        }

    }
    // Print Gantt chart and metrics
    printf("\n=== Gantt Chart ===\n|");
    for(int i = 0; i < total_ticks; i++)
    {
        printf(" %s |", args->processes[gantt_history[i]].pid);
    }
    printf("\n\n");
    for(int i = 0; i < args->task_count; i++)
    {
        total_waiting += args->processes[i].wait_time;
        total_turnaround += args->processes[i].turnaround_time;
        total_response += args->processes[i].response_time;
    }
    printf("=== Metrics ===\n");
    printf("Average Waiting Time: %.2f\n", (double)total_waiting / args->task_count);
    printf("Average Turnaround Time: %.2f\n", (double)total_turnaround / args->task_count);
    printf("Average Response Time: %.2f\n", (double)total_response / args->task_count);
    printf("Total Context Switches: %d\n", context_switches);
    printf("===============\n");
    
    fprintf(log_file, "%s,%d,%.2f,%.2f,%.2f,%d\n", args->workload_file,
                                        quantum, 
                                        (double)total_turnaround / (double)args->task_count, 
                                        (double)total_waiting / args->task_count, 
                                        (double)total_response / args->task_count, 
                                        context_switches);
    fclose(log_file);

    pthread_exit(0);
    return NULL;
}

void *scheduler_priority(void *arg)
{
    SchedulerArgs *args = (SchedulerArgs *)arg; // SchedulerArgs contains the list of processes, task count, and other relevant info
    int current_time = 0; // Keep track of the current time in the simulation
    int completed_tasks = 0; // Count of completed tasks to know when to stop the scheduler
    int context_switches = 0; // Count of context switches for metrics
    int prev_proc_index = -1; // Track the previously running process index to count context switches
    int gantt_history[1000]; // Store the order of process execution for Gantt chart
    int total_ticks = 0; // Total time ticks for Gantt chart
    int proc_index; // Index of the currently selected process to run
    int in_queue[MAXPROCESSES] = {0}; // Track which processes are currently in the ready queue
    float total_waiting = 0, total_turnaround = 0, total_response = 0; // Metrics accumulators
    Process *proc = NULL; // Pointer to the currently running process

    while(completed_tasks < args->task_count)
    {
        for(int i = 0; i < args->task_count; i++)
        {
            // Check for newly arrived processes and add them to the ready queue
            if(args->processes[i].arrival_time <= current_time && !in_queue[i] && args->processes[i].remaining > 0)
            {
                in_queue[i] = 1;
            }
        }
        // Find highest priority process in ready queue
        proc_index = -1;
        for(int i = 0; i < args->task_count; i++)
        {
            // Lower priority value means higher priority
            if(in_queue[i] && (proc_index == -1 || args->processes[i].priority < args->processes[proc_index].priority))
            {
                proc_index = i;
            }
        }
        // If no process is ready, advance time
        if(proc_index == -1)
        {
            current_time++;
            continue;
        }
        proc = &args->processes[proc_index];
        // Count context switch if we are switching to a different process
        if(prev_proc_index != proc_index)
        {
            context_switches++;
        }
        prev_proc_index = proc_index;
        // Record response time for the process if it's starting for the first time
        if(proc->has_started == 0)
        {
            proc->response_time = current_time - proc->arrival_time;
            proc->has_started = 1;
        }
        gantt_history[current_time] = proc_index;
        total_ticks = current_time + 1;
        // Signal the process to run for one time unit
        sem_post(proc->semaphore);
        sem_wait(scheduler_semaphore);
        current_time++;
        // If process has completed, update metrics and remove from ready queue
        if(proc->remaining == 0)
        {
            completed_tasks++;
            in_queue[proc_index] = 0;

            proc->turnaround_time = current_time - proc->arrival_time;
            proc->wait_time = proc->turnaround_time - proc->burst_time;

        }
    }
    printf("\n=== Gantt Chart ===\n|");
    for(int i = 0; i < total_ticks; i++)
    {
        printf(" %s |", args->processes[gantt_history[i]].pid);
    }
    printf("\n\n");
    for(int i = 0; i < args->task_count; i++)
    {
        total_waiting += args->processes[i].wait_time;
        total_turnaround += args->processes[i].turnaround_time;
        total_response += args->processes[i].response_time;
    }
    printf("=== Metrics ===\n");
    printf("Average Waiting Time: %.2f\n", (double)total_waiting / args->task_count);
    printf("Average Turnaround Time: %.2f\n", (double)total_turnaround / args->task_count);
    printf("Average Response Time: %.2f\n", (double)total_response / args->task_count);
    printf("Total Context Switches: %d\n", context_switches);
    printf("===============\n");
    pthread_exit(0);
    return NULL;
}

void *scheduler_cfs(void *arg)
{
    SchedulerArgs *args = (SchedulerArgs *)arg; // SchedulerArgs contains the list of processes, task count, and other relevant info
    int current_time = 0; // Keep track of the current time in the simulation
    int completed_tasks = 0; // Count of completed tasks to know when to stop the scheduler
    int lowest_vruntime_index; // Index of the process with the lowest virtual runtime to run next
    int context_switches = 0; // Count of context switches for metrics
    int prev_proc_index = -1; // Track the previously running process index to count context switches
    int total_ticks = 0; // Total time ticks for Gantt chart
    int gantt_history[1000]; // Store the order of process execution for Gantt chart
    int ready_queue[MAXPROCESSES]; // Track which processes are currently in the ready queue
    int in_queue[MAXPROCESSES] = {0}; // Track which processes are currently in the ready queue
    float total_waiting = 0, total_turnaround = 0, total_response = 0; // Metrics accumulators
    Process *proc; // Pointer to the currently running process
    FILE *log_file; // File pointer for logging results

    log_file = fopen("cfs_results.csv", "a");
    if(!log_file) {
        perror("Error opening log file");
        pthread_exit(0);
    }

    while(completed_tasks < args->task_count)
    {
        for(int i = 0; i < args->task_count; i++)
        {
            // Check for newly arrived processes and add them to the ready queue
            if(args->processes[i].arrival_time <= current_time && !in_queue[i] && args->processes[i].remaining > 0)
            {
                ready_queue[i] = 1;
                in_queue[i] = 1;
            }
        }
        // Find process with lowest virtual runtime
        lowest_vruntime_index = -1;
        for(int i = 0; i < args->task_count; i++)
        {
            if(in_queue[i])
            {
                // Virtual runtime can be approximated as wait_time + burst_time - remaining
                if(lowest_vruntime_index == -1 || (args->processes[i].wait_time + args->processes[i].burst_time - args->processes[i].remaining) < (args->processes[lowest_vruntime_index].wait_time + args->processes[lowest_vruntime_index].burst_time - args->processes[lowest_vruntime_index].remaining))
                {
                    lowest_vruntime_index = i;
                }
            }
        }
        // If no process is ready, advance time
        if(lowest_vruntime_index == -1)
        {
            current_time++;
            continue;
        }
        proc = &args->processes[lowest_vruntime_index];
        // Count context switch if we are switching to a different process
        if(prev_proc_index != lowest_vruntime_index)
        {
            context_switches++;
        }
        prev_proc_index = lowest_vruntime_index;
        // Record response time for the process if it's starting for the first time
        if(proc->has_started == 0)
        {
            proc->response_time = current_time - proc->arrival_time;
            proc->has_started = 1;
        }
        gantt_history[current_time] = lowest_vruntime_index;
        total_ticks = current_time + 1;
        // Signal the process to run for one time unit
        sem_post(proc->semaphore);
        sem_wait(scheduler_semaphore);
        current_time++;
        // If process has completed, update metrics and remove from ready queue
        if(proc->remaining == 0)
        {
            completed_tasks++;
            in_queue[lowest_vruntime_index] = 0;

            proc->turnaround_time = current_time - proc->arrival_time;
            proc->wait_time = proc->turnaround_time - proc->burst_time;

        }
    }
    printf("\n=== Gantt Chart ===\n|");
    for(int i = 0; i < total_ticks; i++)
    {
        printf(" %s |", args->processes[gantt_history[i]].pid);
    }
    printf("\n\n");
    for(int i = 0; i < args->task_count; i++)
    {
        total_waiting += args->processes[i].wait_time;
        total_turnaround += args->processes[i].turnaround_time;
        total_response += args->processes[i].response_time;
    }
    printf("=== Metrics ===\n");
    printf("Average Waiting Time: %.2f\n", (double)total_waiting / args->task_count);
    printf("Average Turnaround Time: %.2f\n", (double)total_turnaround / args->task_count);
    printf("Average Response Time: %.2f\n", (double)total_response / args->task_count);
    printf("Total Context Switches: %d\n", context_switches);
    printf("===============\n");
    
    fprintf(log_file, "%s,%.2f,%.2f,%.2f,%d\n", args->workload_file,
                                        (double)total_turnaround / (double)args->task_count, 
                                        (double)total_waiting / args->task_count, 
                                        (double)total_response / args->task_count, 
                                        context_switches);

    pthread_exit(0);
    return NULL;
}

#endif
