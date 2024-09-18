// SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
#include "user.skel.h"
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
#include <liburing.h>

typedef uint32_t u32;

struct fork_event {
  __u32 pid;
  __u32 ppid;
  char comm[16];
};

struct pread_prepare_info {
    int pid;
    int fd;
    char *dest;
    size_t size;
    uint64_t i_ino;
    size_t offset;
    uint32_t seq_num;
    uint32_t depends_on;
    uint32_t res;
};

struct io_uring ring;

size_t batch_count = 0;

static volatile sig_atomic_t exiting = 0;

static int libbpf_print_fn(enum libbpf_print_level level, const char *format,
                           va_list args) {
  return vfprintf(stderr, format, args);
}

static void sig_int(int signo) { exiting = 1; }

// Function to process pread_prepare_info data
void process_pread_prepare_info(struct pread_prepare_info *info) {
    struct io_uring_sqe *sqe;
    sqe = io_uring_get_sqe(&ring);
    if (!sqe) {
        fprintf(stderr, "Unable to get SQE\n");
        return;
    }

    io_uring_prep_read(sqe, info->fd, info->dest, info->size, info->offset);
    sqe->flags |= IOSQE_IO_LINK;

    batch_count++;
}

int process_pread_submit_info(u32 pid) {
    if (batch_count == 0) return 0;
    io_uring_submit(&ring);

    struct io_uring_cqe *cqe;
    for (int i = 0; i < batch_count; i++) {
        int ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret < 0) {
            fprintf(stderr, "Error waiting for completion: %s\n",
                                                            strerror(-ret));
            return 1;
        }
        if (cqe->res < 0) {
            fprintf(stderr, "Error in async operation: %s\n", strerror(-cqe->res));
        }
        io_uring_cqe_seen(&ring, cqe);
    }
    batch_count = 0;
    return 0;
}

static int handle_pread_prepare(void *ctx, void *data, size_t len) {
    struct pread_prepare_info *info = (struct pread_prepare_info *)data;
    process_pread_prepare_info(info);
    return 0;
}

// Callback function to handle data from pread_submit_batch_ringbuf
static int handle_pread_submit(void *ctx, void *data, size_t len) {
    u32 *pid = (u32 *)data;

    process_pread_submit_info(*pid);
    return 0;
}

int main(int argc, char **argv) {
    LIBBPF_OPTS(bpf_object_open_opts, open_opts);
    struct user_bpf *obj;
    int err;

    libbpf_set_print(libbpf_print_fn);

    obj = user_bpf__open_opts(&open_opts);
    if (!obj) {
        fprintf(stderr, "Failed to open BPF object\n");
        return 1;
    }

    err = user_bpf__load(obj);
    if (err) {
        fprintf(stderr, "Failed to load BPF object: %d\n", err);
        goto cleanup;
    }

    // struct bpf_link *link_readCount = bpf_program__attach_usdt(
    //     obj->progs.readCount, -1,
    //     "/mnt/fast25/ClickHouse/build/programs/clickhouse-server", "clickhouse",
    //     "readCount", NULL);
    // if (!link_readCount) {
    //     err = errno;
    //     fprintf(stderr, "Failed to attach USDT probe 'readCount': %d\n", err);
    //     goto cleanup;
    // }

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
        fprintf(stderr, "Failed to attach USDT probe 'readS': %d\n", err);
        goto cleanup;
    }

    if (signal(SIGINT, sig_int) == SIG_ERR) {
        fprintf(stderr, "Can't set signal handler: %s\n", strerror(errno));
        err = 1;
        goto cleanup;
    }

    struct ring_buffer *rb_pread_prepare = NULL;
    struct ring_buffer *rb_pread_submit = NULL;

    rb_pread_prepare = ring_buffer__new(bpf_map__fd(obj->maps.pread_prepare_ringbuf),
                                        handle_pread_prepare, NULL, NULL);
    if (!rb_pread_prepare) {
        err = -1;
        fprintf(stderr, "Failed to create ring buffer for pread_prepare_ringbuf\n");
        goto cleanup;
    }

    rb_pread_submit = ring_buffer__new(bpf_map__fd(obj->maps.pread_submit_batch_ringbuf),
                                       handle_pread_submit, NULL, NULL);
    if (!rb_pread_submit) {
        err = -1;
        fprintf(stderr, "Failed to create ring buffer for pread_submit_batch_ringbuf\n");
        goto cleanup;
    }

    
    
    int ret = io_uring_queue_init(1024, &ring, 0);
    if (ret) {
        fprintf(stderr, "Unable to setup io_uring: %s\n", strerror(-ret));
        return 1;
    }

    while (!exiting) {
        err = ring_buffer__poll(rb_pread_prepare, -1);
        if (err < 0) {
            fprintf(stderr, "Error polling the pread_prepare ring buffer: %d\n", err);
            break;
        }

        err = ring_buffer__poll(rb_pread_submit, -1);
        if (err < 0) {
            fprintf(stderr, "Error polling the pread_submit ring buffer: %d\n", err);
            break;
        }
    }

cleanup:
    ring_buffer__free(rb_pread_prepare);
    ring_buffer__free(rb_pread_submit);
    user_bpf__destroy(obj);
    io_uring_queue_exit(&ring);
    return err != 0;
}
