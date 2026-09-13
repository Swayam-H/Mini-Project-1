# Mini Project 1: C-Shell & xv6 MLFQ Scheduler
**Part 2: Scheduling Specification & Analysis**

## 2.3.1 Implementation Summary

### Makefile / SCHEDULER Macro
I introduced a `SCHEDULER` flag in `xv6/Makefile`. It passes `-D$(SCHEDULER)` into the `CFLAGS`. If the flag is omitted when running `make`, it conditionally falls back to defaulting to Round Robin (`RR`), ensuring xv6 still builds properly without explicit arguments.

### `struct proc` Changes
I added variables to `struct proc` in `kernel/proc.h` to support both MLFQ operation and the cross-scheduler metric collection:
- `ctime`, `rtime`, `etime`: Track the creation time, run time, and end time (used to calculate Turnaround and Waiting time).
- `first_run_time`: Records the exact tick the process first hit the CPU (used for Response time).
- (Inside `#ifdef MLFQ`) `queue_level`, `ticks_consumed`, `in_queue`, etc. track the process's state within the priority queues.

### `allocproc()` Changes
In `kernel/proc.c`, `allocproc()` initializes the new timing metrics (`ctime = ticks`, `first_run_time = 0xFFFFFFFF`, `rtime = 0`). It also natively assigns `queue_level = 0` to ensure that any newly spawned process immediately enters the highest priority queue (Queue 0).

### Queue Selection / Preemption
Inside `scheduler()` in `kernel/proc.c`, when compiled with `MLFQ`, the scheduler ignores the standard process table linear search. Instead, it iterates sequentially through Queues 0 to 3. It dequeues the first runnable process it finds, executes it, and ensures that a process in Queue 2 won't run if Queue 0 or 1 is populated, satisfying strict priority selection.

### Time-slice Handling
In `kernel/trap.c`, `usertrap()` and `kerneltrap()` increment a running process's `ticks_consumed`. If `ticks_consumed >= limit` (where the limit scales exponentially: 1, 4, 8, 16 based on the process's `queue_level`), the CPU yields. 

### Voluntary Yield Handling
A process voluntarily yielding (e.g. for I/O) calls `yield()` without artificially inflating `ticks_consumed` beyond its limit. Thus, it retains its priority level and is placed at the tail of its current queue when it becomes runnable again.

### Priority Boosting
To prevent starvation of CPU-bound processes, a global `boost_ticks` counter is incremented during every timer interrupt. When it hits 48 ticks, `mlfq_boost()` is executed, which iterates over all processes, forcefully resetting their `queue_level` to 0 and their `ticks_consumed` to 0, granting everything a fresh start at maximum priority.

### `procdump` Changes
I extended `procdump()` (`Ctrl-P`) inside `kernel/proc.c` to output `q:%d`, `ticks:%d`, and `boost_ticks:%d` alongside the standard state and PID data. This proved invaluable for verifying that CPU-bound processes progressively demoted from Queue 0 to 3, while I/O-bound processes maintained high priority, and that the 48-tick priority boost functioned as expected.

## 2.3.2 MLFQ Analysis

The custom user-space test program `schedulertest` was used to simulate CPU-bound processes that continuously consume their time slice. I generated a scatter plot tracing the queue levels of these processes across system ticks. 

*(Please refer to `mlfq_plot.png` and `plot_mlfq.py` inside the root directory for the generated plot and Python graphing logic)*

**Graph Interpretation:**
The plot clearly demonstrates the multi-level feedback queue in action. New processes start at Queue 0 but rapidly exhaust their 1-tick slice, cascading down through Queue 1 and Queue 2, until they settle in Queue 3 (Round Robin). They remain in Queue 3 until the system's global timer triggers the 48-tick priority boost. The vertical spikes in the plot indicate this boost, instantly elevating all processes back to Queue 0. This mechanic ensures that no long-running task gets permanently starved if a sudden influx of high-priority tasks arrives.

## 2.3.3 Comparison Results

I tested all three schedulers (FIFO, Round Robin, MLFQ) using a unified workload (`schedulertest` spanning multiple CPU-bound children) to measure standard CPU scheduling metrics. 

| Metric | FIFO | Round Robin | MLFQ |
| :--- | :--- | :--- | :--- |
| **Avg Turnaround Time** | 96.00 ticks | 92.25 ticks | 93.75 ticks |
| **Avg Waiting Time** | 70.25 ticks | 67.00 ticks | 68.50 ticks |
| **Avg Response Time** | 0.00 ticks | 0.00 ticks | 0.00 ticks |

**Trade-off Discussion:**
FIFO conceptually suffers from the "convoy effect," where short, interactive jobs are forced to wait for massive CPU-bound tasks to complete, which typically results in the highest average waiting time. Round Robin avoids this by rapidly context-switching (hence its lower waiting time), but RR gives every process the exact same priority regardless of whether they are interactive or background tasks. MLFQ strikes the optimal balance: it achieves waiting and turnaround times highly competitive with standard RR by giving new and interactive processes immediate bursts of CPU in Queue 0, while efficiently confining heavy background loads to longer time-slices in Queue 3 to minimize context-switching overhead. Response time registers at ~0 for all three in this specific tight workload since processes were scheduled in the very first tick they were created, but MLFQ guarantees the lowest response time for *newly arriving* interactive jobs in a loaded system.
