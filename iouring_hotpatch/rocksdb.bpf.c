/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2022, eunomia-bpf org
 * All rights reserved.
 */
#include "rocksdb.h"
#include "vmlinux.h"
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
// #include <linux/io_uring.h>
// #include <liburing.h>

#define EXTENDED_HELPER_IOURING_INIT 1006
#define EXTENDED_HELPER_IOURING_SUBMIT_WRITE 1007
#define EXTENDED_HELPER_IOURING_SUBMIT_READ 1008
#define EXTENDED_HELPER_IOURING_SUBMIT_SEND 1009
#define EXTENDED_HELPER_IOURING_SUBMIT_RECV 1010
#define EXTENDED_HELPER_IOURING_SUBMIT_FSYNC 1011
#define EXTENDED_HELPER_IOURING_WAIT_AND_SEEN 1012
#define EXTENDED_HELPER_IOURING_SUBMIT 1013

static long (*io_uring_init_global)(void) = (void *)
    EXTENDED_HELPER_IOURING_INIT;
static long (*io_uring_submit_write)(int fd, char *buf,
                                     unsigned long long size) = (void *)
    EXTENDED_HELPER_IOURING_SUBMIT_WRITE;
static long (*io_uring_submit_read)(int fd, char *buf, unsigned long long size,
                                    unsigned long long offset) = (void *)
    EXTENDED_HELPER_IOURING_SUBMIT_READ;
static long (*io_uring_submit_send)(int fd, char *buf, unsigned long long size,
                                    int flags) = (void *)
    EXTENDED_HELPER_IOURING_SUBMIT_SEND;
static long (*io_uring_submit_recv)(int fd, char *buf, unsigned long long size,
                                    int flags) = (void *)
    EXTENDED_HELPER_IOURING_SUBMIT_RECV;
static long (*io_uring_submit_fsync)(int fd) = (void *)
    EXTENDED_HELPER_IOURING_SUBMIT_FSYNC;
static long (*io_uring_wait_and_seen)(void) = (void *)
    EXTENDED_HELPER_IOURING_WAIT_AND_SEEN;
static long (*io_uring_submit)(void) = (void *)EXTENDED_HELPER_IOURING_SUBMIT;
enum ReplicaStatus {

};
 struct raft_ctr_state {

  enum ReplicaStatus state;
  int myIdx, leaderIdx, batchSize;
  uint64_t view, lastOp;
};

struct bpf_uring_ctx {
  char *data;
  int fd;
  int done;
  uint64_t next_addr[16];
  uint64_t size[16];
  struct raft_ctr_state next_state[16];
  char *scratch;
} struct {
  __uint(type, BPF_MAP_TYPE_HASH);
  __type(key, u32);                    // i_ino
  __type(value, struct bpf_uring_ctx); // data
  __uint(max_entries, 10000);
  __uint(map_flags, 0);
} bpftime_inode_map SEC(".maps");

int patch_size = 1e8;
int current_count = 0;
bool global = false;

SEC("uprobe//lib/x86_64-linux-gnu/libc.so.6:write")
int BPF_UPROBE(write, int __fd, const void *__buf, unsigned long long __n) {
  bpf_printk("write called");
  if (global && __fd != 1) {
    if (current_count < patch_size) {
      io_uring_submit_write(__fd, __buf, __n);
      current_count++;
    } else {
      io_uring_submit_write(__fd, __buf, __n);
      current_count++;
      io_uring_submit();
      for (int i = 0; i < current_count; i++) {
        io_uring_wait_and_seen();
      }
      current_count = 0;
    }
    bpf_override_return(0, __n);
  }
  // bpf_printk("write called");
  // bpf_override_return(0, 5);
  return 0;
}

SEC("uprobe//lib/x86_64-linux-gnu/libc.so.6:fsync")
int BPF_UPROBE(fsync, int __fd) {
  if (current_count < patch_size) {
    io_uring_submit_fsync(__fd);
    current_count++;
  } else {
    io_uring_submit_fsync(__fd);
    current_count++;
    io_uring_submit();
    for (int i = 0; i < current_count; i++) {
      io_uring_wait_and_seen();
    }
    current_count = 0;
  }
  bpf_override_return(0, 0);
  return 0;
}
// patch in higher level
SEC("uprobe//lib/x86_64-linux-gnu/libc.so.6:send")
int BPF_UPROBE(send, int __fd, const void *__buf, unsigned long long __n,
               int __flags) {
  io_uring_submit_send(__fd, __buf, __n, __flags);
  io_uring_submit();
  if (current_count < patch_size) {
    io_uring_submit_fsync(__fd);
    current_count++;
  } else {
    io_uring_submit_fsync(__fd);
    current_count++;
    io_uring_submit();
    for (int i = 0; i < current_count; i++) {
      io_uring_wait_and_seen();
    }
    current_count = 0;
  }
  return 0;
}

SEC("uprobe//lib/x86_64-linux-gnu/libc.so.6:read")
int BPF_UPROBE(send, int __fd, const void *__buf, unsigned long long __n,
               int __flags) {
  io_uring_submit_read(__fd, __buf, __n, __flags);
  char * data =bpf_mem_alloc(__n);
  bpf_map_update_elem(&bpftime_inode_map, &__fd, &data, BPF_ANY);
  io_uring_submit();
  if (current_count < patch_size) {
    io_uring_submit_fsync(__fd);
    current_count++;
  } else {
    io_uring_submit_fsync(__fd);
    current_count++;
    io_uring_submit();
    for (int i = 0; i < current_count; i++) {
      io_uring_wait_and_seen();
    }
    current_count = 0;
  }
  return 0;
}

SEC("uprobe//mnt/fast25/clickhouse-uring/iouring_hotpatch/fsync_write:start")
int BPF_UPROBE(start, int __fd) {
  bpf_printk("start called and init io_uring\n");
  global = true;
  io_uring_init_global();
  return 0;
}

SEC("kretprobe/bpf_uring_enable")
int BPF_KPROBE(bpf_uring_enable) {
  int tgid = bpf_get_current_pid_tgid();
  int pid = tgid >> 32;

  bpf_override_return(0, true);
  return 0;
}
char LICENSE[] SEC("license") = "GPL";
