#ifndef SCHEDSIM_H
#define SCHEDSIM_H

#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/wait.h>

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

typedef struct 
{
    int quantum;
    int current_time;
    int proc_index;
    int completed_tasks;
    int context_switches;
    float total_waiting, total_turnaround, total_response;
    int prev_proc_index;
    int gantt_history[1000]; // Store the order of process execution for Gantt chart
    int total_ticks;
    int ticks_run; 
    int ready_queue[MAXPROCESSES];
    int head, tail, q_size;
    int in_queue[MAXPROCESSES];
    Process *proc;
    FILE *log_file;
    
} RR_vars;
void rr_vars_init(RR_vars *vars);
void rr_vars_init(RR_vars *vars)
{
    vars->quantum = 0;
    vars->current_time = 0;
    vars->proc_index = 0;
    vars->completed_tasks = 0;
    vars->context_switches = 0;
    vars->total_waiting = 0;
    vars->total_turnaround = 0;
    vars->total_response = 0;
    vars->prev_proc_index = -1;
    memset(vars->gantt_history, -1, sizeof(vars->gantt_history));
    vars->total_ticks = 0;
    vars->ticks_run = 0; 
    memset(vars->ready_queue, -1, sizeof(vars->ready_queue));
    vars->head = 0;
    vars->tail = 0;
    vars->q_size = 0;
    memset(vars->in_queue, 0, sizeof(vars->in_queue));
    vars->proc = NULL;
    vars->log_file = NULL;
}

void *scheduler_rr(void *arg)
{
    SchedulerArgs *args = (SchedulerArgs *)arg;
    RR_vars vars;
    // int quantum = args->quantum;
    // int current_time = 0;
    // int proc_index = 0;
    // int completed_tasks = 0;
    // int context_switches = 0;
    // float total_waiting = 0, total_turnaround = 0, total_response = 0;
    // int prev_proc_index = -1;
    // int gantt_history[1000]; // Store the order of process execution for Gantt chart
    // int total_ticks = 0;
    // int ticks_run = 0; 
    // int ready_queue[MAXPROCESSES];
    // int head = 0, tail = 0, q_size = 0;
    // int in_queue[MAXPROCESSES] = {0};
    // Process *proc;
    // FILE *log_file;

    vars.log_file = fopen("rr_results.csv", "a");
    if (!vars.log_file) {
        perror("Error opening log file");
        pthread_exit(0);
    }

    while(vars.completed_tasks < args->task_count)
    {
        for(int i = 0; i < args->task_count; i++)
        {
            // Check for newly arrived processes and add them to the ready queue
            if(args->processes[i].arrival_time <= vars.current_time && !vars.in_queue[i] && args->processes[i].remaining > 0)
            {
                vars.ready_queue[vars.tail] = i;
                vars.tail = (vars.tail + 1) % MAXPROCESSES;
                vars.q_size++;
                vars.in_queue[i] = 1;
            }
        }
        // If queue is empty CPU is idle advance time
        if(vars.q_size == 0)
        {
            vars.current_time++;
            continue;
        }
        // Dequeue next process to run
        vars.proc_index = vars.ready_queue[vars.head];
        vars.head = (vars.head + 1) % MAXPROCESSES;
        vars.q_size--;
        vars.proc = &args->processes[vars.proc_index];
        // Signal the process to run for one time unit
        vars.ticks_run = 0;
        while(vars.ticks_run < vars.quantum && args->processes[vars.proc_index].remaining > 0)
        {
            //
            if(vars.prev_proc_index != vars.proc_index)
            {
                vars.context_switches++;
            }
            vars.prev_proc_index = vars.proc_index;
            if(vars.proc->has_started == 0)
            {
                vars.proc->response_time = vars.current_time - vars.proc->arrival_time;
                vars.proc->has_started = 1;
            }
            vars.gantt_history[vars.current_time] = vars.proc_index;
            vars.total_ticks = vars.current_time + 1;
            
            sem_post(vars.proc->semaphore);
            sem_wait(scheduler_semaphore);
            vars.ticks_run++;
            vars.current_time++;
            for(int i = 0; i < args->task_count; i++)
            {
                if(args->processes[i].arrival_time <= vars.current_time && !vars.in_queue[i] && args->processes[i].remaining > 0)
                {
                    vars.ready_queue[vars.tail] = i;
                    vars.tail = (vars.tail + 1) % MAXPROCESSES;
                    vars.q_size++;
                    vars.in_queue[i] = 1;
                }
            }
        }
        // time slice
        if (vars.proc->remaining > 0)
        {
            vars.ready_queue[vars.tail] = vars.proc_index;
            vars.tail = (vars.tail + 1) % MAXPROCESSES;
            vars.q_size++;
        }
        else
        {
            vars.completed_tasks++;
            vars.in_queue[vars.proc_index] = 0;

            vars.proc->turnaround_time = vars.current_time - vars.proc->arrival_time;
            vars.proc->wait_time = vars.proc->turnaround_time - vars.proc->burst_time;

            printf("Process %s completed at time %d, Wait Time: %d, Turnaround Time: %d\n", vars.proc->pid, vars.current_time, vars.proc->wait_time, vars.proc->turnaround_time);
        }

    }
    
    printf("\n=== Gantt Chart ===\n|");
    for(int i = 0; i < vars.total_ticks; i++)
    {
        printf(" %s |", args->processes[vars.gantt_history[i]].pid);
    }
    printf("\n\n");
    for(int i = 0; i < args->task_count; i++)
    {
        vars.total_waiting += args->processes[i].wait_time;
        vars.total_turnaround += args->processes[i].turnaround_time;
        vars.total_response += args->processes[i].response_time;
    }
    printf("=== Metrics ===\n");
    printf("Average Waiting Time: %.2f\n", (double)vars.total_waiting / args->task_count);
    printf("Average Turnaround Time: %.2f\n", (double)vars.total_turnaround / args->task_count);
    printf("Average Response Time: %.2f\n", (double)vars.total_response / args->task_count);
    printf("Total Context Switches: %d\n", vars.context_switches);
    printf("===============\n");

    //taskset,quantum,avg_turnaround,avg_waiting,avg_response,context_switches
    fprintf(vars.log_file, "%s,%d,%.2f,%.2f,%.2f,%d\n", args->workload_file,
                                        vars.quantum, 
                                        (double)vars.total_turnaround / (double)args->task_count, 
                                        (double)vars.total_waiting / args->task_count, 
                                        (double)vars.total_response / args->task_count, 
                                        vars.context_switches);
    fclose(vars.log_file);

    pthread_exit(0);
    return NULL;
}

void *scheduler_priority(void *arg)
{
    SchedulerArgs *args = (SchedulerArgs *)arg;
    int current_time = 0;
    int completed_tasks = 0;
    int context_switches = 0;
    int prev_proc_index = -1;
    int gantt_history[1000]; // Store the order of process execution for Gantt chart
    int total_ticks = 0;
    int proc_index;
    int in_queue[MAXPROCESSES] = {0};
    float total_waiting = 0, total_turnaround = 0, total_response = 0;
    Process *proc = NULL;

    while(completed_tasks < args->task_count)
    {
        for(int i = 0; i < args->task_count; i++)
        {
            if(args->processes[i].arrival_time <= current_time && !in_queue[i] && args->processes[i].remaining > 0)
            {
                in_queue[i] = 1;
            }
        }
        // Find highest priority process in ready queue
        proc_index = -1;
        for(int i = 0; i < args->task_count; i++)
        {
            if(in_queue[i] && (proc_index == -1 || args->processes[i].priority < args->processes[proc_index].priority))
            {
                proc_index = i;
            }
        }
        if(proc_index == -1)
        {
            current_time++;
            continue;
        }
        proc = &args->processes[proc_index];
        //
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
        gantt_history[current_time] = proc_index;
        total_ticks = current_time + 1;
        //
        sem_post(proc->semaphore);
        sem_wait(scheduler_semaphore);
        current_time++;
        if(proc->remaining == 0)
        {
            completed_tasks++;
            in_queue[proc_index] = 0;

            proc->turnaround_time = current_time - proc->arrival_time;
            proc->wait_time = proc->turnaround_time - proc->burst_time;

            printf("Process %s completed at time %d, Wait Time: %d, Turnaround Time: %d\n", proc->pid, current_time, proc->wait_time, proc->turnaround_time);
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
    SchedulerArgs *args = (SchedulerArgs *)arg;
    int current_time = 0;
    int completed_tasks = 0;
    int lowest_vruntime_index;
    int context_switches = 0;
    int prev_proc_index = -1;
    float total_waiting = 0, total_turnaround = 0, total_response = 0;
    int gantt_history[1000]; // Store the order of process execution for Gantt chart
    int total_ticks = 0;
    int ready_queue[MAXPROCESSES];
    int in_queue[MAXPROCESSES] = {0};
    Process *proc;
    while(completed_tasks < args->task_count)
    {
        for(int i = 0; i < args->task_count; i++)
        {
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
                if(lowest_vruntime_index == -1 || (args->processes[i].wait_time + args->processes[i].burst_time - args->processes[i].remaining) < (args->processes[lowest_vruntime_index].wait_time + args->processes[lowest_vruntime_index].burst_time - args->processes[lowest_vruntime_index].remaining))
                {
                    lowest_vruntime_index = i;
                }
            }
        }
        if(lowest_vruntime_index == -1)
        {
            current_time++;
            continue;
        }
        proc = &args->processes[lowest_vruntime_index];
        //
        if(prev_proc_index != lowest_vruntime_index)
        {
            context_switches++;
        }
        prev_proc_index = lowest_vruntime_index;
        if(proc->has_started == 0)
        {
            proc->response_time = current_time - proc->arrival_time;
            proc->has_started = 1;
        }
        gantt_history[current_time] = lowest_vruntime_index;
        total_ticks = current_time + 1;
        //
        sem_post(proc->semaphore);
        sem_wait(scheduler_semaphore);
        current_time++;
        if(proc->remaining == 0)
        {
            completed_tasks++;
            in_queue[lowest_vruntime_index] = 0;

            proc->turnaround_time = current_time - proc->arrival_time;
            proc->wait_time = proc->turnaround_time - proc->burst_time;

            printf("Process %s completed at time %d, Wait Time: %d, Turnaround Time: %d\n", proc->pid, current_time, proc->wait_time, proc->turnaround_time);
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

#endif
