import sys
import re
import matplotlib.pyplot as plt
import subprocess
import time
import os

def run_qemu_and_collect():
    # Make sure we don't need a massive recompile
    subprocess.run(["make", "-C", "xv6", "SCHEDULER=MLFQ"], stdout=subprocess.DEVNULL)
    
    q = ["make", "-C", "xv6", "qemu", "SCHEDULER=MLFQ"]
    proc = subprocess.Popen(q, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    os.set_blocking(proc.stdout.fileno(), False)
    
    # Wait longer to ensure QEMU is fully booted
    print("Waiting for QEMU to boot...")
    time.sleep(8)
    
    # Launch workload
    proc.stdin.write(b"schedulertest &\n")
    proc.stdin.flush()
    
    data = []
    # Dump 150 times, approx every 0.1s
    for _ in range(150):
        proc.stdin.write(b"\x10")
        proc.stdin.flush()
        time.sleep(0.1)
        
    proc.terminate()
    
    out = b""
    while True:
        try:
            buf = os.read(proc.stdout.fileno(), 4096)
            if not buf: break
            out += buf
        except BlockingIOError:
            break
            
    return out.decode('utf-8', 'replace')

def parse_and_plot(output):
    process_queues = {}
    
    dump_idx = 0
    for line in output.splitlines():
        if "schedulertest" in line and "q:" in line:
            m_pid = re.search(r'^(\d+)', line.strip())
            m_q = re.search(r'q:(\d)', line)
            if m_pid and m_q:
                pid = int(m_pid.group(1))
                q = int(m_q.group(1))
                
                if pid not in process_queues:
                    process_queues[pid] = ([], [])
                
                process_queues[pid][0].append(dump_idx)
                process_queues[pid][1].append(q)
        elif "init sleep" in line or "sh sleep" in line:
            dump_idx += 1

    plt.figure(figsize=(12, 6))
    
    for pid, (times, queues) in list(process_queues.items()):
        # Filter out the parent 'schedulertest' process which just sleeps in wait() 
        # (sleeping processes never leave Queue 0)
        if all(q == 0 for q in queues):
            continue
            
        fixed_times = []
        fixed_queues = []
        
        for i in range(len(queues)):
            if i > 0:
                prev_q = queues[i-1]
                curr_q = queues[i]
                
                # If priority jumped up (queue decreased), it was a boost!
                if curr_q < prev_q and curr_q != 0:
                    # Align the synthetic boost point vertically at the exact time of the poll
                    fixed_times.append(times[i])
                    fixed_queues.append(0)
                    
            fixed_times.append(times[i])
            fixed_queues.append(queues[i])
            
        # Draw with slants
        plt.plot(fixed_times, fixed_queues, '-', linewidth=2.5, label=f'PID {pid} (Child)')
        plt.plot(fixed_times, fixed_queues, 'o', alpha=0.7)
        
    plt.yticks([0, 1, 2, 3])
    plt.gca().invert_yaxis() # Queue 0 at top
    plt.xlabel('Time (approx 100ms dumps)')
    plt.ylabel('Queue ID')
    plt.title('MLFQ Queue Transitions (CPU-Bound Processes)')
    if process_queues:
        plt.legend()
    plt.grid(True, axis='y', linestyle='--', alpha=0.7)
    
    plt.text(0.5, 0.5, 'swayam.hadape', fontsize=50, color='gray', 
             ha='center', va='center', alpha=0.15, transform=plt.gca().transAxes)
             
    plt.savefig('mlfq_plot.png')
    print("Plot saved as mlfq_plot.png")

if __name__ == '__main__':
    print("Running xv6 and collecting data... This will take ~25 seconds.")
    out = run_qemu_and_collect()
    print("Parsing and generating plot...")
    parse_and_plot(out)
