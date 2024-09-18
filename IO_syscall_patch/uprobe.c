// SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
// se
// #include "attach_override.h"
#include "uprobe.skel.h"
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

int main(int argc, char **argv) {
  LIBBPF_OPTS(bpf_object_open_opts, open_opts);
  struct uprobe_bpf *obj;
  int err;

  libbpf_set_print(libbpf_print_fn);

  obj = uprobe_bpf__open_opts(&open_opts);
  if (!obj) {
    fprintf(stderr, "Failed to open BPF object\n");
    return 1;
  }

  err = uprobe_bpf__load(obj);
  if (err) {
    fprintf(stderr, "Failed to load BPF object: %d\n", err);
    goto cleanup;
  }

    // err = bpf_prog_attach_uprobe_with_override(
    //     bpf_program__fd(obj->progs.overrideTrace),
    //     "/mnt/fast25/ClickHouse/build/programs/clickhouse",
    //     "_ZNK2DB10ReadBuffer13overrideTraceENSt3__112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEEmPcPmmPbS7_Pl");
    // if (err) {
    //   fprintf(
    //       stderr,
    //       "Failed to attach BPF program to DB::ReadBuffer::readStrict
    //       (enter)\n");
    //   goto cleanup;
    // }

  // attach the above uprobe without overriding
  struct bpf_link* link = bpf_program__attach_uprobe(
      obj->progs.overrideTrace, false, -1,
      "/mnt/fast25/ClickHouse/build/programs/clickhouse",
      0xc76fd80);
  if (!link) {
    fprintf(stderr, "Failed to attach BPF program to "
                    "DB::ReadBuffer::overrideTrace (enter)\n");
    goto cleanup;
  }

  if (signal(SIGINT, sig_int) == SIG_ERR) {
    fprintf(stderr, "Can't set signal handler: %s\n", strerror(errno));
    err = 1;
    goto cleanup;
  }

  while (!exiting) {
    sleep(1);
  }

cleanup:
  // ring_buffer__free(rb);
  uprobe_bpf__destroy(obj);
  return err != 0;
}
