// SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
// se
#include "batch.skel.h"
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
  struct batch_bpf *obj;
  int err;

  libbpf_set_print(libbpf_print_fn);

  obj = batch_bpf__open_opts(&open_opts);
  if (!obj) {
    fprintf(stderr, "Failed to open BPF object\n");
    return 1;
  }

  err = batch_bpf__load(obj);
  if (err) {
    fprintf(stderr, "Failed to load BPF object: %d\n", err);
    goto cleanup;
  }

  struct bpf_link *link_readCount = bpf_program__attach_usdt(
      obj->progs.readCount, -1,
      "/mnt/fast25/ClickHouse/build/programs/clickhouse-server", "clickhouse",
      "readCount", NULL);
  if (!link_readCount) {
    err = errno;
    fprintf(stderr, "Failed to attach USDT probe 'readCount': %d\n", err);
    goto cleanup;
  }

  struct bpf_link *link_preadRaw = bpf_program__attach_usdt(
      obj->progs.preadRaw, -1,
      "/mnt/fast25/ClickHouse/build/programs/clickhouse", "clickhouse",
      "preadRaw", NULL);
  if (!link_preadRaw) {
    err = errno;
    fprintf(stderr, "Failed to attach USDT probe 'preadRaw': %d (%s)\n", err,
            strerror(err));
    goto cleanup;
  }

  struct bpf_link *link_batchSubmit = bpf_program__attach_usdt(
      obj->progs.batchSubmit, -1,
      "/mnt/fast25/ClickHouse/build/programs/clickhouse", "clickhouse",
      "batchSubmit", NULL);
  if (!link_batchSubmit) {
    err = errno;
    fprintf(stderr, "Failed to attach USDT probe 'batchSubmit': %d\n", err);
    goto cleanup;
  }

  struct bpf_link *link_readS = bpf_program__attach_usdt(
      obj->progs.readS, -1, "/mnt/fast25/ClickHouse/build/programs/clickhouse",
      "clickhouse", "readS", NULL);
  if (!link_readS) {
    err = errno;
    fprintf(stderr, "Failed to attach USDT probe 'batchSubmit': %d\n", err);
    goto cleanup;
  }

  struct bpf_link *link_main_entry = bpf_program__attach_usdt(
      obj->progs.main_entry, -1,
      "/mnt/fast25/ClickHouse/build/programs/clickhouse", "clickhouse",
      "batchSubmit", NULL);
  if (!link_main_entry) { 
    err = errno;  
    fprintf(stderr, "Failed to attach USDT probe 'batchSubmit': %d\n", err);
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
  batch_bpf__destroy(obj);
  return err != 0;
}
