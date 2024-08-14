// SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
#include "attach_override.h"
#include "clickhouse-uring.skel.h"
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
#include <linux/types.h>

static volatile sig_atomic_t exiting = 0;

static int libbpf_print_fn(enum libbpf_print_level level, const char *format,
                           va_list args) {
  return vfprintf(stderr, format, args);
}

static void sig_int(int signo) { exiting = 1; }

int main(int argc, char **argv) {
  LIBBPF_OPTS(bpf_object_open_opts, open_opts);
  struct clickhouse_uring_bpf *obj;
  int err;
  int map_fd;

  libbpf_set_print(libbpf_print_fn);

  obj = clickhouse_uring_bpf__open_opts(&open_opts);
  if (!obj) {
    fprintf(stderr, "Failed to open BPF object\n");
    return 1;
  }

  err = clickhouse_uring_bpf__load(obj);
  if (err) {
    fprintf(stderr, "Failed to load BPF object: %d\n", err);
    goto cleanup;
  }

  err = bpf_prog_attach_uprobe_with_override(
        bpf_program__fd(obj->progs.bpf_start_patch),
        "/mnt/fast25/ClickHouse/build/programs/clickhouse-server", "_ZN2DB16ThreadPoolReader6submitENS_19IAsynchronousReader7RequestE");

  if (err) {
    fprintf(stderr, "Failed to attach BPF program to open\n");
    goto cleanup;
  }

  // struct bpf_link *attach = bpf_program__attach_uprobe(
  //     obj->progs.bpf_start_patch, 
  //     true, 
  //     -1,
  //     "/mnt/fast25/ClickHouse/build/programs/clickhouse", 
  //     0xc3e51a0);

  // if (!attach) {
  //     fprintf(stderr, "Failed to attach BPF program to isClickhouseApp (enter)\n");
  //     err = -1;
  //     goto cleanup;
  // }

  // struct bpf_link *attac = bpf_program__attach_uprobe(
  //     obj->progs.bpf_start_patch, 
  //     true, 
  //     -1,
  //     "/mnt/fast25/ClickHouse/build/programs/clickhouse", 
  //     0x15646ce0);

  // if (!attac) {
  //     fprintf(stderr, "Failed to attach BPF program to isClickhouseApp (enter)\n");
  //     err = -1;
  //     goto cleanup;
  // }

  // struct bpf_link *atta = bpf_program__attach_uprobe(
  //     obj->progs.bpf_start_patch, 
  //     true, 
  //     -1,
  //     "/mnt/fast25/ClickHouse/build/programs/clickhouse", 
  //     0x1aa80c60);

  // if (!atta) {
  //     fprintf(stderr, "Failed to attach BPF program to isClickhouseApp (enter)\n");
  //     err = -1;
  //     goto cleanup;
  // }
  
  err = bpf_prog_attach_uprobe_with_override(
    bpf_program__fd(obj->progs.bpf_start_patch),
    "/mnt/fast25/ClickHouse/build/programs/clickhouse-server", "_ZN2DB16ThreadPoolReader4waitEv");
  if (err) {
      fprintf(stderr, "Failed to attach BPF program to DB::ReadBuffer::next (enter)\n");
      goto cleanup;
  }

  err = bpf_prog_attach_uprobe_with_override(
    bpf_program__fd(obj->progs.bpf_start_patch),
    "/mnt/fast25/ClickHouse/build/programs/clickhouse-server", "_ZN2DB6Server3runEv");
  if (err) {
      fprintf(stderr, "Failed to attach BPF program to DB::ReadBuffer::next (enter)\n");
      goto cleanup;
  }

  err = bpf_prog_attach_uprobe_with_override(
    bpf_program__fd(obj->progs.bpf_start_patch),
    "/mnt/fast25/ClickHouse/build/programs/clickhouse-server", "_ZN2DB24MergeTreeSelectProcessor4readEv");
  if (err) {
      fprintf(stderr, "Failed to attach BPF program to DB::ReadBuffer::next (enter)\n");
      goto cleanup;
  }

  err = bpf_prog_attach_uprobe_with_override(
    bpf_program__fd(obj->progs.bpf_start_patch),
    "/mnt/fast25/ClickHouse/build/programs/clickhouse-server", "_ZN2DB28CompressedReadBufferFromFile20setReadUntilPositionEm");
  if (err) {
      fprintf(stderr, "Failed to attach BPF program to DB::ReadBuffer::next (enter)\n");
      goto cleanup;
  }

  

  // err = bpf_prog_attach_uprobe_with_override(
  //     bpf_program__fd(obj->progs.bpf_start_patch),
  //     "/mnt/fast25/clickhouse-uring/IO_syscall_patch/victim", "main");

  err = bpf_prog_attach_uprobe_with_override(
      bpf_program__fd(obj->progs.bpf_write_patch),
      "/lib/x86_64-linux-gnu/libc.so.6", "write");
  if (err) {
    fprintf(stderr, "Failed to attach BPF program to write\n");
    goto cleanup;
  }


  err = bpf_prog_attach_uprobe_with_override(
      bpf_program__fd(obj->progs.bpf_start_patch),
      "/mnt/fast25/ClickHouse/build/programs/clickhouse", "_ZN2DB10ReadBuffer10readStrictEPcm");
  if (err) {
      fprintf(stderr, "Failed to attach BPF program to DB::ReadBuffer::readStrict (enter)\n");
      goto cleanup;
  }

  // err = bpf_prog_attach_uprobe_with_override(
  //   bpf_program__fd(obj->progs.bpf_start_patch),
  //   "/mnt/fast25/ClickHouse/build/programs/clickhouse", "_Z15isClickhouseAppNSt3__117basic_string_viewIcNS_11char_traitsIcEEEERNS_6vectorIPcNS_9allocatorIS5_EEEE");
  // if (err) {
  //     fprintf(stderr, "Failed to attach BPF program to DB::ReadBuffer::next (enter)\n");
  //     goto cleanup;
  // }

  // err = bpf_prog_attach_uprobe_with_override(
  //     bpf_program__fd(obj->progs.bpf_next_exit),
  //     "/mnt/fast25/ClickHouse/build/programs/clickhouse", "DB::ReadBuffer::next");
  // if (err) {
  //     fprintf(stderr, "Failed to attach BPF program to DB::ReadBuffer::next (exit)\n");
  //     goto cleanup;
  // }

  // err = bpf_prog_attach_uprobe_with_override(
  //     bpf_program__fd(obj->progs.bpf_ignore_enter),
  //     "/mnt/fast25/ClickHouse/build/programs/clickhouse", "DB::ReadBuffer::ignore");
  // if (err) {
  //     fprintf(stderr, "Failed to attach BPF program to DB::ReadBuffer::ignore (enter)\n");
  //     goto cleanup;
  // }

  // err = bpf_prog_attach_uprobe_with_override(
  //     bpf_program__fd(obj->progs.bpf_ignore_exit),
  //     "/mnt/fast25/ClickHouse/build/programs/clickhouse", "DB::ReadBuffer::ignore");
  // if (err) {
  //     fprintf(stderr, "Failed to attach BPF program to DB::ReadBuffer::ignore (exit)\n");
  //     goto cleanup;
  // }

  // err = bpf_prog_attach_uprobe_with_override(
  //     bpf_program__fd(obj->progs.bpf_read_enter),
  //     "/mnt/fast25/ClickHouse/build/programs/clickhouse", "DB::ReadBuffer::read");
  // if (err) {
  //     fprintf(stderr, "Failed to attach BPF program to DB::ReadBuffer::read (enter)\n");
  //     goto cleanup;
  // }

  // err = bpf_prog_attach_uprobe_with_override(
  //     bpf_program__fd(obj->progs.bpf_read_exit),
  //     "/mnt/fast25/ClickHouse/build/programs/clickhouse", "DB::ReadBuffer::read");
  // if (err) {
  //     fprintf(stderr, "Failed to attach BPF program to DB::ReadBuffer::read (exit)\n");
  //     goto cleanup;
  // }


  // err = bpf_prog_attach_uprobe_with_override(
  //     bpf_program__fd(obj->progs.bpf_readStrict_exit),
  //     "/mnt/fast25/ClickHouse/build/programs/clickhouse", "DB::ReadBuffer::readStrict");
  // if (err) {
  //     fprintf(stderr, "Failed to attach BPF program to DB::ReadBuffer::readStrict (exit)\n");
  //     goto cleanup;
  // }

  // err = bpf_prog_attach_uprobe_with_override(
  //     bpf_program__fd(obj->progs.bpf_read_patch),
  //     "/mnt/fast25/clickhouse-uring/IO_syscall_patch/test_speed", "wrapper");
  // if (err) {
  //   fprintf(stderr, "Failed to attach BPF program to read\n");
  //   goto cleanup;
  // }

  // err = bpf_prog_attach_uprobe_with_override(
  //     bpf_program__fd(obj->progs.bpf_pread_patch),
  //     "/lib/x86_64-linux-gnu/libc.so.6", "pread");
  // if (err) {
  //   fprintf(stderr, "Failed to attach BPF program to pread\n");
  //   goto cleanup;
  // }

  // err = bpf_prog_attach_uprobe_with_override(
  //     bpf_program__fd(obj->progs.bpf_start_patch),
  //     "/lib/x86_64-linux-gnu/libc.so.6", "read");
  // if (err) {
  //   fprintf(stderr, "Failed to attach BPF program to read\n");
  //   goto cleanup;
  // }
  // err = bpf_prog_attach_uprobe_with_override(
  //     bpf_program__fd(obj->progs.bpf_start_patch),
  //     "/lib/x86_64-linux-gnu/libc.so.6", "write");
  // if (err) {
  //   fprintf(stderr, "Failed to attach BPF program to send\n");
  //   goto cleanup;
  // }

  // err = bpf_prog_attach_uprobe_with_override(
  //     bpf_program__fd(obj->progs.bpf_recv_patch),
  //     "/lib/x86_64-linux-gnu/libc.so.6", "recv");
  // if (err) {
  //   fprintf(stderr, "Failed to attach BPF program to recv\n");
  //   goto cleanup;
  // }

  // err = bpf_prog_attach_uprobe_with_override(
  //     bpf_program__fd(obj->progs.bpf_end_patch),
  //     "/mnt/fast25/clickhouse-uring/IO_syscall_patch/test_speed", "end_mark");
  // if (err) {
  //   fprintf(stderr, "Failed to attach BPF program to read\n");
  //   goto cleanup;
  // }

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

  unsigned key = 0, value = 79;
  if (bpf_map_update_elem(map_fd, &key, &value, BPF_ANY)) {
      fprintf(stderr, "ERROR: Updating op_count map failed\n");
      return 1;
  }



  while (!exiting) {
      unsigned op_count_value;
      int res = bpf_map_lookup_elem(map_fd, &key, &op_count_value);
      if (res == 0) {
          printf("op_count: %u\n", op_count_value);
      } else {
          fprintf(stderr, "ERROR: Reading op_count map failed\n");
      }
      sleep(1);
  }

cleanup:
  clickhouse_uring_bpf__destroy(obj);
  return err != 0;
}
