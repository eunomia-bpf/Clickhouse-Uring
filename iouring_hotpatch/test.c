// SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
// Copyright (c) 2019 Facebook
// Copyright (c) 2020 Netflix
//
// Based on test(8) from BCC by Brendan Gregg and others.
// 14-Feb-2020   Brendan Gregg   Created this.

#include "attach_override.h"
#include "test.skel.h"
#include <argp.h>
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t exiting = 0;

static int libbpf_print_fn(enum libbpf_print_level level, const char *format,
                           va_list args) {
  return vfprintf(stderr, format, args);
}

static void sig_int(int signo) { exiting = 1; }

int main(int argc, char **argv) {
  LIBBPF_OPTS(bpf_object_open_opts, open_opts);
  // struct perf_buffer *pb = NULL;
  struct test_bpf *obj;
  int err;

  libbpf_set_print(libbpf_print_fn);

  obj = test_bpf__open_opts(&open_opts);
  if (!obj) {
    fprintf(stderr, "failed to open BPF object\n");
    return 1;
  }

  /* initialize global data (filtering options) */

  err = test_bpf__load(obj);
  if (err) {
    fprintf(stderr, "failed to load BPF object: %d\n", err);
    goto cleanup;
  }
  err = bpf_prog_attach_uprobe_with_override(
      bpf_program__fd(obj->progs.start),
      "/mnt/fast25/clickhouse-uring/iouring_hotpatch/read", "start");
  if (err) {
    fprintf(stderr, "Failed to attach BPF program start\n");
    goto cleanup;
  }

//   err = bpf_prog_attach_uprobe_with_override(bpf_program__fd(obj->progs.fsync),
//                                              "/lib/x86_64-linux-gnu/libc.so.6",
//                                              "fsync");
//   if (err) {
//     fprintf(stderr, "Failed to attach BPF program fsync\n");
//     goto cleanup;
//   }
//   err = bpf_prog_attach_uprobe_with_override(bpf_program__fd(obj->progs.write),
//                                              "/lib/x86_64-linux-gnu/libc.so.6",
//                                              "write");
//   if (err) {
//     fprintf(stderr, "Failed to attach BPF program write\n");
//     goto cleanup;
//   }
  err = bpf_prog_attach_uprobe_with_override(bpf_program__fd(obj->progs.pread),
                                             "/lib/x86_64-linux-gnu/libc.so.6",
                                             "pread");
  if (err) {
    fprintf(stderr, "Failed to attach BPF program read\n");
    goto cleanup;
  }

//   err = bpf_prog_attach_uprobe_with_override(bpf_program__fd(obj->progs.send),
//                                              "/lib/x86_64-linux-gnu/libc.so.6",
//                                              "send");
//   if (err) {
//     fprintf(stderr, "Failed to attach BPF program send\n");
//     goto cleanup;
//   }

//   err = bpf_prog_attach_uprobe_with_override(bpf_program__fd(obj->progs.recv),
//                                              "/lib/x86_64-linux-gnu/libc.so.6",
//                                              "recv");
//   if (err) {
//     fprintf(stderr, "Failed to attach BPF program recv\n");
//     goto cleanup;
//   }
  if (signal(SIGINT, sig_int) == SIG_ERR) {
    fprintf(stderr, "can't set signal handler: %s\n", strerror(errno));
    err = 1;
    goto cleanup;
  }

  /* main: poll */
  while (!exiting) {
  }

cleanup:
  test_bpf__destroy(obj);

  return err != 0;
}