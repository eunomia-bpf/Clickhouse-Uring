#!/usr/bin/env python3

from bcc import BPF, USDT
import subprocess
import time

# Define BPF program
bpf_text = """
#include <uapi/linux/ptrace.h>
int trace_submit(struct pt_regs *ctx) {
    bpf_trace_printk("submit called\\n");
    return 0;
}

int trace_wait(struct pt_regs *ctx) {
    bpf_trace_printk("wait called\\n");
    return 0;
}

int trace_before_read(struct pt_regs *ctx) {
    bpf_trace_printk("before_read_from_task called\\n");
    return 0;
}

int trace_read_strict(struct pt_regs *ctx) {
    bpf_trace_printk("readStrict called\\n");
    return 0;
}
"""

# Compile and load BPF program
b = BPF(text=bpf_text)

# Function to attach uprobes to a given process

# Function to attach USDT probes to a given process
def attach_usdt_probes(pid):
    try:
        usdt = USDT(pid=pid)
        usdt.enable_probe(probe="before_read_from_task", fn_name="trace_before_read")
        usdt.enable_probe(probe="readStrict", fn_name="trace_read_strict")
        b.attach_usdt(usdt)
        print(f"USDT probes attached to PID {pid}")
    except Exception as e:
        print(f"Failed to attach USDT probes to PID {pid}: {e}")

# Start the ClickHouse server instances and attach probes
servers = [
    "/mnt/fast25/ClickHouse/build/programs/clickhouse-server --config-file=/mnt/fast25/ch_cluster/chnode1/config/config.xml",
    "/mnt/fast25/ClickHouse/build/programs/clickhouse-server --config-file=/mnt/fast25/ch_cluster/chnode2/config/config.xml",
    "/mnt/fast25/ClickHouse/build/programs/clickhouse-server --config-file=/mnt/fast25/ch_cluster/chnode3/config/config.xml"
]

pids = []
for server in servers:
    proc = subprocess.Popen(server.split(), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    pids.append(proc.pid)

# Allow some time for the processes to start
time.sleep(15)  # Increase delay if necessary

# Attach probes to each ClickHouse server instance
for pid in pids:
    attach_usdt_probes(pid)

print("Probes attached. Monitoring trace output...")

# Print BPF trace output
try:
    b.trace_print()
except KeyboardInterrupt:
    print("Detaching probes and exiting...")
