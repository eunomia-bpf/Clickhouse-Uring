// SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
// se
#include "tp.skel.h"
#include <argp.h>
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <linux/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
// #include <bpf/bpf_helpers.h>

struct fork_event {
  __u32 pid;
  __u32 ppid;
  char comm[16];
};

static volatile sig_atomic_t exiting = 0;

static int libbpf_print_fn(enum libbpf_print_level level, const char *format,
                           va_list args) {
  return vfprintf(stderr, format, args);
}

static void sig_int(int signo) { exiting = 1; }

static int handle_event(void *ctx, void *data, size_t data_sz) {
  struct fork_event *event = data;
  printf("Forked process: PID %u, PPID %u, Comm %s\n", event->pid, event->ppid,
         event->comm);
  return 0;
}

// static int add_sample_event(struct ring_buffer *rb) {
//   struct fork_event *event = bpf_ringbuf_reserve(rb, sizeof(*event), 0);
//   if (!event) {
//     return -1;
//   }

//   event->pid = 1234;
//   event->ppid = 5678; // Sample PPID
//   strncpy(event->comm, "sample_comm", sizeof(event->comm));

//   bpf_ringbuf_submit(event, 0);
//   return 0;
// }



int main(int argc, char **argv) {
  LIBBPF_OPTS(bpf_object_open_opts, open_opts);
  struct tp_bpf *obj;
  int err;
  int map_fd;

  libbpf_set_print(verbose_print);

  obj = tp_bpf__open_opts(&open_opts);
  if (!obj) {
    fprintf(stderr, "Failed to open BPF object\n");
    return 1;
  }

  err = tp_bpf__load(obj);
  if (err) {
    fprintf(stderr, "Failed to load BPF object: %d\n", err);
    goto cleanup;
  }

  // err = bpf_prog_attach_uprobe_with_override(
  //     bpf_program__fd(obj->progs.read_from_task),
  //     "/mnt/fast25/ClickHouse/build/programs/clickhouse-server",
  //     "_ZN2DB24MergeTreeSelectProcessor4readEv");
  // if (err) {
  //   fprintf(stderr,
  //           "Failed to attach BPF program to DB::ReadBuffer::next
  //           (enter)\n");
  //   goto cleanup;
  // }

  struct bpf_link *link = bpf_program__attach_usdt(
      obj->progs.read_from_task, -1,
      "/mnt/fast25/ClickHouse/build/programs/clickhouse", "clickhouse",
      "before_read_from_task", NULL);
  if (!link) {
    fprintf(stderr,
            "Failed to attach USDT probe 'before_read_from_task':% d\n ", err);
    goto cleanup;
  }

  link = bpf_program__attach_usdt(
      obj->progs.read_bytes_count, -1,
      "/mnt/fast25/ClickHouse/build/programs/clickhouse", "clickhouse",
      "bytesRead", NULL);
  if (!link) {
    fprintf(stderr,
            "Failed to attach USDT probe 'bytesRead':% d\n ", err);
    goto cleanup;
  }

  // link = bpf_program__attach_usdt(
  //     obj->progs.read_from_task, -1,
  //     "/mnt/fast25/ClickHouse/build/programs/clickhouse", "clickhouse",
  //     "after_read_strict", NULL);
  // if (!link) {
  //   fprintf(stderr, "Failed to attach USDT probe 'after_read_strict': %d\n",
  //           err);
  //   goto cleanup;
  // }

  // link = bpf_program__attach_tracepoint(obj->progs.trace_sched_process_fork,
  //                                       "sched", "sched_process_fork");
  // if (!link) {
  //   fprintf(
  //       stderr,
  //       "Failed to attach tracepoint program 'trace_sched_process_fork':
  //       %d\n", err);
  //   goto cleanup;
  // }

  printf("fork tracepoint attached\n");

  if (signal(SIGINT, sig_int) == SIG_ERR) {
    fprintf(stderr, "Can't set signal handler: %s\n", strerror(errno));
    err = 1;
    goto cleanup;
  }

  map_fd = bpf_map__fd(obj->maps.op_count);
  if (map_fd < 0) {
    fprintf(stderr, "Failed to find BPF map\n");
    return 1;
  }
  printf("Map found\n");

  struct ring_buffer *rb = ring_buffer__new(bpf_map__fd(obj->maps.ringbuf),
                                            handle_event, NULL, NULL);
  if (!rb) {
    fprintf(stderr, "Failed to create ring buffer\n");
    err = 1;
    goto cleanup;
  }

  printf("Ring buffer created\n");

  unsigned key = 0;
  while (!exiting) {
    sleep(1);
    unsigned op_count_value;
    int res = bpf_map_lookup_elem(map_fd, &key, &op_count_value);
    if (res == 0) {
      printf("op_count: %u\n", op_count_value);
    } else {
      fprintf(stderr, "ERROR: Reading op_count map failed\n");
    }

    err = ring_buffer__poll(rb, 100);
    if (err < 0) {
      fprintf(stderr, "Error polling ring buffer: %d\n", err);
      break;
    }
  }

cleanup:
  ring_buffer__free(rb);
  tp_bpf__destroy(obj);
  return err != 0;
}