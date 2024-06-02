/* SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2022, eunomia-bpf org
 * All rights reserved.
 */
#include "vmlinux.h"
#include "rocksdb.h"
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

static long (*io_uring_init_global)(void) = (void *) EXTENDED_HELPER_IOURING_INIT;
static long (*io_uring_submit_write)(int fd, char *buf, unsigned long long size) = (void *) EXTENDED_HELPER_IOURING_SUBMIT_WRITE;
static long (*io_uring_submit_read)(int fd, char *buf, unsigned long long size, unsigned long long offset) = (void *) EXTENDED_HELPER_IOURING_SUBMIT_READ;
static long (*io_uring_submit_send)(int fd, char *buf, unsigned long long size, int flags) = (void *) EXTENDED_HELPER_IOURING_SUBMIT_SEND;
static long (*io_uring_submit_recv)(int fd, char *buf, unsigned long long size,int flags) = (void *) EXTENDED_HELPER_IOURING_SUBMIT_RECV;
static long (*io_uring_submit_fsync)(int fd) = (void *) EXTENDED_HELPER_IOURING_SUBMIT_FSYNC;
static long (*io_uring_wait_and_seen)(void) = (void *) EXTENDED_HELPER_IOURING_WAIT_AND_SEEN;
static long (*io_uring_submit)(void) = (void *) EXTENDED_HELPER_IOURING_SUBMIT;

struct uring_check_data{
  int pid;
  int fd;
  u32 i_ino;
};

struct {
  __uint(type, BPF_MAP_TYPE_HASH);
  __type(key, u32);                       // i_ino
  __type(value, struct uring_check_data); // data
  __uint(max_entries, 10000);
  __uint(map_flags, 0);
} bpftime_inode_map SEC(".maps");
struct {
  __uint(type, BPF_MAP_TYPE_ARRAY);
  __type(key, u32);                       // i_ino
  __type(value, struct uring_check_data); // data
  __uint(max_entries, 10000);
  __uint(map_flags, 0);
} bpftime_dependent_array SEC(".maps");

int patch_size = 48;
int current_count = 0;

SEC("uprobe//lib/x86_64-linux-gnu/libc.so.6:write")
int BPF_UPROBE(write, int __fd, const void *__buf, unsigned long long __n) {
  if (__n == 5) {
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
    bpf_override_return(0, 5);
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

// SEC("uprobe//lib/x86_64-linux-gnu/libc.so.6:send")
// int BPF_UPROBE(send, int __fd, const void *__buf, unsigned long long __n, int __flags) {
//     io_uring_submit_send(__fd, __buf, __n, __flags);
//     io_uring_submit();

//     return 0;
// }

SEC("uprobe//mnt/fast25/clickhouse-uring/iouring_hotpatch/fsync_write:start")
int BPF_UPROBE(start, int __fd) {
  bpf_printk("start called and init io_uring\n");
  io_uring_init_global();
  return 0;
}

int successful_writeback_count = 0;
int current_pid = 0;

SEC("kretprobe/file_write_and_wait_range")
int BPF_KPROBE(file_write_and_wait_range) {
  int pid = (bpf_get_current_pid_tgid() >> 32);
  if (pid != current_pid) {
    return 0;
  }
  // bpf_printk("__writeback_single_inode, %lu successful_writeback_count: %llu",
  //            i_ino, successful_writeback_count);
  // bpf_printk("data: %d, %d, %lu", data->pid, data->fd, data->i_ino);
  __sync_fetch_and_add(&successful_writeback_count, 1);
  return 0;
}
// batching
SEC("kprobe/__writeback_single_inode")
int BPF_KPROBE(__writeback_single_inode, struct inode *inode,
               struct writeback_control *wbc) {
  int pid = bpf_get_current_pid_tgid() >> 32;
  if (!inode) {
    // bpf_printk("inode is null");
    return 0;
  }
  long unsigned int i_ino = BPF_CORE_READ(inode, i_ino);
  struct uring_check_data *data =
      bpf_map_lookup_elem(&bpftime_inode_map, &i_ino);
  if (!data) {
    // bpf_printk("data is null");
    return 0;
  }
  bpf_printk("__writeback_single_inode, %lu successful_writeback_count: %llu",
             i_ino, successful_writeback_count); // batching
  
  bpf_printk("data: %d, %d, %lu", data->pid, data->fd, data->i_ino);
  successful_writeback_count++;
  return 0;
}
char LICENSE[] SEC("license") = "GPL";
