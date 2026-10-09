# CPU Scheduling Simulator

## Overview
This project is a multi-threaded CPU scheduling simulator written in C. It uses `pthreads` and named semaphores to simulate an operating system scheduler managing multiple processes. The project implements a "puppet-master" model where a central scheduler thread controls the execution of individual process threads tick-by-tick. A tick is represented by one loop iteration. 

### Implemented Algorithms
1. **Round Robin (RR)**: Preemptive scheduling with a configurable time quantum.
2. **Preemptive Priority**: Preempts currently running tasks if a task with a lower numerical priority value arrives.
3. **Simplified Completely Fair Scheduler (CFS)**: Selects the process with the lowest accumulated virtual runtime.

---

## How to Compile
Ensure you have `gcc` installed.

```bash
gcc schedsim.c -o schedsim -Wall -Wextra -pthread
```

## How to Run
The program takes in command line arguments for the input file or if applicable, the time quantum.

./schedsim <algorithm> <input_file> [time_quantum]
### Valid Algorithm Arguments 
1. **rr**: Round Robin (requires time quantum argument) 
2. **priority**: Preemptive Priority
3. **cfs**: Simplified CFS

##### Examples:
./schedsim rr tasks.txt 4
./schedsim priorty tasks.txt
./schedsim cfs tasks.txt

### Input File Format 
The program reads text from an input file that is formatted with headers that are ignored. Only the data is used in the program. 

##### Example file:
```text
PID Arrival Burst Priority
P1  0       8     3
P2  1       4     1
P3  2       9     4
P4  3       5     2
```

### Sample Outputs

1. **Round Robin (Quantum = 4) using the example file above**
```text
% ./schedsim rr tasks.txt 4

=== Gantt Chart ===
| P1 | P1 | P1 | P1 | P2 | P2 | P2 | P2 | P3 | P3 | P3 | P3 | P4 | P4 | P4 | P4 | P1 | P1 | P1 | P1 | P3 | P3 | P3 | P3 | P4 | P3 |

=== Metrics ===
Average Waiting Time: 11.75
Average Turnaround Time: 18.25
Average Response Time: 4.50
Total Context Switches: 8
===============
```

2. **Preemptive Priority using the example file above**:
```text
% ./schedsim priority tasks.txt 

=== Gantt Chart ===
| P1 | P2 | P2 | P2 | P2 | P4 | P4 | P4 | P4 | P4 | P1 | P1 | P1 | P1 | P1 | P1 | P1 | P3 | P3 | P3 | P3 | P3 | P3 | P3 | P3 | P3 |

=== Metrics ===
Average Waiting Time: 6.50
Average Turnaround Time: 13.00
Average Response Time: 4.25
Total Context Switches: 5
===============
```
3. **Simplified Completely Fair Scheduler using the example file above**:
```text
% ./schedsim cfs tasks.txt 

=== Gantt Chart ===
| P1 | P1 | P3 | P3 | P3 | P3 | P3 | P3 | P3 | P3 | P3 | P1 | P1 | P1 | P1 | P1 | P1 | P4 | P4 | P4 | P4 | P4 | P2 | P2 | P2 | P2 |

=== Metrics ===
Average Waiting Time: 11.00
Average Turnaround Time: 17.50
Average Response Time: 8.75
Total Context Switches: 5
===============
```

### Data Analysis Report
**The Effect of Time Quantum on Round Robin**
*Tested Quanta: 1, 2, 4, and 8*

1. **Turnaround Time:** The average turnaround time decreased as the time quantum increased. This would mean that shorter tasks are able to finish first due to less context switches.

2. **Response Time:** The smaller the quantum the shorter the response time. This is due to the increase in context switches when using a smaller time quantum. This does increase overhead do to the increase in context switches.  

3. **Context Switches:** With the larger quantum there are many less context switches, with a quantum of 8 there are only 5 switches and with a quantum of 1 there are 24 switches. 

**Algorithm Comparison**
1. **Waiting Time Winner:** Priority has the lower waiting time compared to CFS. This can be due to setting priorities for tasks that need more time to run whereas CFS allows all to run for the same amount of time. 

2. **Handling of Short Tasks:** [Explain how CFS and RR handle short tasks naturally, whereas Priority relies entirely on the assigned priority number.]
CFS and RR handle shorts tasks well because the time quantum usually isn't super large and the time given for each in CFS is never super long. Priority relies on the priority of each task, so if a task that needs lots of time and has high priority then it will run before shorter tasks can. 

3. **Preemption Behavior:** [Discuss how often Preemptive Priority interrupted tasks compared to CFS. Which one felt more "disruptive" to running processes?]
Priority scheduling had many fewer interruptions compared to CFS. CFS is much more disruptive than priority because 

### Conclusion
For a general-purpose operating system, CFS or Round Robin are typically the best choices because they prioritize fairness and prevent task starvation, which is crucial for a more streamlined user experience. However, different environments dictate different needs: Round Robin is excellent for UI responsiveness, while Preemptive Priority is essential for real-time systems where critical tasks must be executed immediately regardless of fairness.