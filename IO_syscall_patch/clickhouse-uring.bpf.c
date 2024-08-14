// SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)

#include "vmlinux.h"
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

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
int bpf_readImpl_patch(struct pt_regs *ctx) {
  // int fd = PT_REGS_PARM1(ctx);
  // void *buf = (void *)PT_REGS_PARM2(ctx);
  // size_t n = PT_REGS_PARM3(ctx);

  // // figure out a way to increment clickhouse metrics

  // if (!global || fd < 3) {
  //   return 0;
  // }

  // struct uring_check_data op = {.pid = bpf_get_current_pid_tgid() >> 32,
  //                               .fd = fd,
  //                               .i_ino = (u64)buf,
  //                               .depends_on = 0,
  //                               .res = 0};

  // int ret = io_uring_submit_read(fd, buf, n, -1);

  // io_uring_set_linkflag();
  // struct uring_check_data *op_stale = add_pending_op(&op);

  // bpf_override_return(ctx, n);
  bpf_printk("DB::DiskAccessStorage::readImpl\n");
  // bpf_override_return(ctx, 9999);
  return 0;
}

// SEC("uprobe/DB::ReadBuffer::readStrict_enter")
// int bpf_readStrict_enter(struct pt_regs *ctx) {
//     bpf_printk("Entering DB::ReadBuffer::readStrict\n");
//     return 0;
// }

// SEC("uprobe/DB::ReadBuffer::next_enter")
// int bpf_next_enter(struct pt_regs *ctx) {
//     bpf_printk("Entering DB::ReadBuffer::next\n");
//     return 0;
// }

// SEC("uretprobe/DB::ReadBuffer::next_exit")
// int bpf_next_exit(struct pt_regs *ctx) {
//     bpf_printk("Exiting DB::ReadBuffer::next\n");
//     return 0;
// }

// SEC("uprobe/DB::ReadBuffer::ignore_enter")
// int bpf_ignore_enter(struct pt_regs *ctx) {
//     bpf_printk("Entering DB::ReadBuffer::ignore\n");
//     return 0;
// }

// SEC("uretprobe/DB::ReadBuffer::ignore_exit")
// int bpf_ignore_exit(struct pt_regs *ctx) {
//     bpf_printk("Exiting DB::ReadBuffer::ignore\n");
//     return 0;
// }

// SEC("uprobe/DB::ReadBuffer::read_enter")
// int bpf_read_enter(struct pt_regs *ctx) {
//     bpf_printk("Entering DB::ReadBuffer::read\n");
//     return 0;
// }

// SEC("uretprobe/DB::ReadBuffer::read_exit")
// int bpf_read_exit(struct pt_regs *ctx) {
//     bpf_printk("Exiting DB::ReadBuffer::read\n");
//     return 0;
// }

// SEC("uretprobe/DB::ReadBuffer::readStrict_exit")
// int bpf_readStrict_exit(struct pt_regs *ctx) {
//     bpf_printk("Exiting DB::ReadBuffer::readStrict\n");
//     return 0;
// }

// SEC("uprobe")
// int bpf_end_patch(struct pt_regs *ctx) {
//   bpf_printk("unbatched ops submitted\n");
//   handle_unbatched_ops();

//   return 0;
// }

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

// SEC("uprobe/libc.so.6:pread")
// int bpf_pread_patch(struct pt_regs *ctx) {
//   int fd = PT_REGS_PARM1(ctx);
//   void *buf = (void *)PT_REGS_PARM2(ctx);
//   size_t n = PT_REGS_PARM3(ctx);
//   off_t offset = PT_REGS_PARM4(ctx);

//   if (!global || fd < 3) {
//     return 0;
//   }
//   bpf_printk("pread called with fd: %d\n, content: %s\n", fd, buf);
//   struct uring_check_data op = {.pid = bpf_get_current_pid_tgid() >> 32,
//                                 .fd = fd,
//                                 .i_ino = (u64)buf,
//                                 .depends_on = 0,
//                                 .res = 0};

//   int ret = io_uring_submit_pread(fd, buf, n, offset);
//   if (ret) {
//     return 0;
//   }

//   io_uring_set_linkflag();
//   add_pending_op(&op);

// }

SEC("uprobe")
int bpf_write_patch(struct pt_regs *ctx) {
  u32 key = 0;

  u32 *count = bpf_map_lookup_elem(&op_count, &key);
  if (count) {
      *count += 1;
      bpf_map_update_elem(&op_count, &key, count, BPF_ANY);
  }

  int fd = PT_REGS_PARM1(ctx);
  const void *buf = (const void *)PT_REGS_PARM2(ctx);
  size_t n = PT_REGS_PARM3(ctx);

  if (!global || fd < 3) {
    return 0;
  }

  struct uring_check_data op = {.pid = bpf_get_current_pid_tgid() >> 32,
                                .fd = fd,
                                .i_ino = (u64)buf,
                                .depends_on = 0,
                                .res = 0};

  // int ret = io_uring_submit_write(fd, buf, n);

  // io_uring_set_linkflag();
  bpf_printk("write called with fd: %d\n, content: %s\n", fd, buf);

  // struct uring_check_data *op_stale = add_pending_op(&op);
  // bpf_override_return(ctx, n);

  // bpf_printk("write called\n");
  // bpf_override_return(ctx, 9999);
  return 0;
}

char LICENSE[] SEC("license") = "GPL";