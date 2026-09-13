# OSN Mini Project 1: C-Shell & xv6 MLFQ

**Author:** Swayam Hadape

## Overview
This repository contains two major components for the OSN Mini Project 1:
1. **C-Shell (`c-shell/`)**: A custom, POSIX-compliant UNIX shell built from scratch in C, featuring custom lexing/parsing, background job control, file redirection, piping, and exotic built-in commands (`hop`, `reveal`, `peek`, `snoop`, etc.).
2. **xv6 MLFQ Scheduler (`xv6/`)**: A heavily modified xv6 kernel incorporating a Multi-Level Feedback Queue (MLFQ) scheduler with 4 queues, dynamic time-slicing, and anti-starvation priority boosting. Includes a fallback Round Robin (RR) and a custom FIFO implementation for statistical comparison.

## Part 1: C-Shell
### Compilation and Execution
To build and run the C-Shell:
```bash
cd c-shell
make all
./shell.out
```
To clean the compiled object files and the executable:
```bash
make clean
```

### Notes & Assumptions
- **Frecency Storage**: The `hop` command's frecency algorithm stores its persistent directory data inside a hidden file located at `~/.cshell_frecency`.
- **Background Processes**: Background process completion messages are safely reaped in the background but are printed *right before the next prompt is drawn* (matching Bash behavior) to prevent overwriting active user input.

## Part 2: xv6 MLFQ Scheduler
### Compilation and Execution
To compile and boot xv6 with the MLFQ scheduler:
```bash
cd xv6
make qemu SCHEDULER=MLFQ
```
*(You can also use `SCHEDULER=FIFO` or `SCHEDULER=RR`. If the flag is omitted, it defaults to Round Robin).*

### Generating the MLFQ Graph
The Python script used to generate the required scatter plot for Section 2.3.2 is located in the root directory. To generate the plot (`mlfq_plot.png`), ensure you have `matplotlib` installed and run:
```bash
python3 plot_mlfq.py
```
This script will automatically compile xv6 with MLFQ, boot QEMU, run the custom `schedulertest` workload, and plot the queue transitions.

### Report
The comparative analysis and implementation details for the xv6 schedulers can be found in `xv6/report.md`.
