// SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)

#include "vmlinux.h"
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/usdt.bpf.h>

int global = false;

#define EXTENDED_HELPER_IOURING_INIT 1006
#define EXTENDED_HELPER_IOURING_SUBMIT_WRITE 1007
#define EXTENDED_HELPER_IOURING_SUBMIT_READ 1008
#define EXTENDED_HELPER_IOURING_SUBMIT_SEND 1009
#define EXTENDED_HELPER_IOURING_SUBMIT_RECV 1010
#define EXTENDED_HELPER_IOURING_SUBMIT_FSYNC 1011
#define EXTENDED_HELPER_IOURING_WAIT_AND_SEEN 1012
#define EXTENDED_HELPER_IOURING_SUBMIT 1013
#define EXTENDED_HELPER_IOURING_SUBMIT_AND_WAIT 1014
#define EXTENDED_HELPER_IOURING_SUBMIT_PREAD 1015
#define EXTENDED_HELPER_IOURING_SET_LINKFLAG 1016
#define EXTENDED_HELPER_IOURING_WAIT 1017
#define EXTENDED_HELPER_IOURING_SEEN 1018

static long (*io_uring_init_global)(void) = (void *)
    EXTENDED_HELPER_IOURING_INIT;
static long (*io_uring_submit_write)(int fd, const void *buf,
                                     unsigned long long size) = (void *)
    EXTENDED_HELPER_IOURING_SUBMIT_WRITE;
static long (*io_uring_submit_read)(int fd, char *buf, unsigned long long size,
                                    unsigned long long offset) = (void *)
    EXTENDED_HELPER_IOURING_SUBMIT_READ;
static long (*io_uring_submit_send)(int fd, const void *buf,
                                    unsigned long long size, int flags) =
    (void *)EXTENDED_HELPER_IOURING_SUBMIT_SEND;
static long (*io_uring_submit_recv)(int fd, void *buf, unsigned long long size,
                                    int flags) = (void *)
    EXTENDED_HELPER_IOURING_SUBMIT_RECV;
static long (*io_uring_submit_fsync)(int fd) = (void *)
    EXTENDED_HELPER_IOURING_SUBMIT_FSYNC;
static long (*io_uring_wait_and_seen)(void) = (void *)
    EXTENDED_HELPER_IOURING_WAIT_AND_SEEN;
static long (*io_uring_usubmit)(void) = (void *)EXTENDED_HELPER_IOURING_SUBMIT;
static long (*io_uring_usubmit_and_wait)(unsigned int min_complete) = (void *)
    EXTENDED_HELPER_IOURING_SUBMIT_AND_WAIT;
static long (*io_uring_submit_pread)(int fd, void *buf, unsigned long long size,
                                     unsigned long long offset) = (void *)
    EXTENDED_HELPER_IOURING_SUBMIT_PREAD;
static long (*io_uring_set_linkflag)(void) = (void *)
    EXTENDED_HELPER_IOURING_SET_LINKFLAG;
static long (*io_uring_wait)(void) = (void *)EXTENDED_HELPER_IOURING_WAIT;
static long (*io_uring_seen)(void) = (void *)EXTENDED_HELPER_IOURING_SEEN;

#define BATCH_SIZE 1024

struct uring_check_data {
  int pid;
  int fd;
  u64 i_ino;
  u32 seq_num;
  u32 depends_on;
  u_int res;
};

struct fork_event {
  __u32 pid;
  __u32 ppid;
  char comm[16];
};

struct readBytes {
  __u32 min_size;
  __u32 max_size;
};

struct {
  __uint(type, BPF_MAP_TYPE_ARRAY);
  __uint(max_entries, 1);
  __type(key, u32);
  __type(value, u32);
} op_count SEC(".maps");

struct {
  __uint(type, BPF_MAP_TYPE_ARRAY);
  __uint(max_entries, 1);
  __type(key, u32);
  __type(value, u32);
} global_seq_num SEC(".maps");

struct {
  __uint(type, BPF_MAP_TYPE_HASH);
  __type(key, u32);                       // i_ino
  __type(value, struct uring_check_data); // data
  __uint(max_entries, 10000);
  __uint(map_flags, 0);
} bpftime_inode_map SEC(".maps");

struct {
  __uint(type, BPF_MAP_TYPE_RINGBUF);
  __uint(max_entries, 4096);
} ringbuf SEC(".maps");

static __always_inline int handle_unbatched_ops() {
  u32 index = 0;
  u32 *count = bpf_map_lookup_elem(&op_count, &index);

  io_uring_usubmit();
  for (int i = 0; i < *count; i++) {
    io_uring_wait_and_seen();
  }

  u32 zero = 0;
  bpf_map_update_elem(&op_count, &index, &zero, BPF_ANY);
  return 0;
}

static __always_inline int submit_if_batch_full(void) {
  u32 index = 0;
  u32 *count = bpf_map_lookup_elem(&op_count, &index);

  if (!count || *count < BATCH_SIZE) {
    return 0;
  }

  io_uring_usubmit();
  for (int i = 0; i < *count; i++) {
    io_uring_wait_and_seen();

    // struct uring_check_data *op = bpf_map_lookup_elem(&bpftime_inode_map,
    // &i);

    // bpf_printk("op: %d\n", op->res);
  }

  return 1;
}

static __always_inline struct uring_check_data *
add_pending_op(struct uring_check_data *op) {

  u32 index = 0;
  u32 *count = bpf_map_lookup_elem(&op_count, &index);
  u32 *seq = bpf_map_lookup_elem(&global_seq_num, &index);

  op->seq_num = (*seq)++;
  bpf_map_update_elem(&global_seq_num, &index, seq, BPF_ANY);

  // bpf_map_update_elem(&bpftime_dependent_array, count, op, BPF_ANY);
  (*count)++;
  bpf_map_update_elem(&op_count, &index, count, BPF_ANY);
  bpf_map_update_elem(&bpftime_inode_map, count, op, BPF_ANY);

  int res = submit_if_batch_full();

  struct uring_check_data *op_stale =
      bpf_map_lookup_elem(&bpftime_inode_map, count);
  if (!op_stale) {
    bpf_printk("op_stale is NULL\n");
    return NULL;
  }
  // bpf_printk("op_stale: %d\n", op_stale->res);
  if (res) {
    u32 zero = 0;
    bpf_map_update_elem(&op_count, &index, &zero, BPF_ANY);
  }
  return op_stale;
}

SEC("uprobe")
int bpf_start_patch(struct pt_regs *ctx) {
  bpf_printk("DB::Server::main called\n");
  u32 key = 0;

  u32 *count = bpf_map_lookup_elem(&op_count, &key);
  if (count) {
    *count += 1;
    bpf_map_update_elem(&op_count, &key, count, BPF_ANY);
  }
  // io_uring_init_global();
  global = true;
  // bpf_override_return(ctx, -1);
  return 0;
}

SEC("usdt/clickhouse/bytesRead")
int read_bytes_count(struct pt_regs *ctx) {

  return 0;
}

SEC("usdt/clickhouse/before_read_from_task")
int read_from_task(struct pt_regs *ctx) {
  u32 key = 0;

  u32 *count = bpf_map_lookup_elem(&op_count, &key);
  if (count) {
    *count += 1;
    bpf_map_update_elem(&op_count, &key, count, BPF_ANY);
  }
  return 0;
}

SEC("usdt/clickhouse/after_read_strict")
int read_strict(struct pt_regs *ctx) {
  u32 key = 0;

  u32 *count = bpf_map_lookup_elem(&op_count, &key);
  if (count) {
    *count += 1;
    bpf_map_update_elem(&op_count, &key, count, BPF_ANY);
  }
  return 0;
}

SEC("tracepoint/sched/sched_process_fork")
int trace_sched_process_fork(struct trace_event_raw_sched_process_fork *ctx) {
  struct fork_event *event;
  event = bpf_ringbuf_reserve(&ringbuf, sizeof(*event), 0);
  if (!event) {
    return 0;
  }

  bpf_printk("fork event: pid: %d, ppid: %d, comm: %s\n", ctx->child_pid,
             ctx->parent_pid, ctx->child_comm);
  event->pid = ctx->child_pid;
  event->ppid = ctx->parent_pid;
  bpf_probe_read_kernel_str(&event->comm, sizeof(event->comm), ctx->child_comm);

  bpf_ringbuf_submit(event, 0);
  return 0;
}

char LICENSE[] SEC("license") = "GPL";