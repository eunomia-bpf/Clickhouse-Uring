#include "vmlinux.h"
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/usdt.bpf.h>

int global = false;
int global_seq = 0;

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
#define EXTENDED_HELPER_IOURING_SET_LINK 1016
#define EXTENDED_HELPER_IOURING_WAIT 1017
#define EXTENDED_HELPER_IOURING_SEEN 1018
#define EXTENDED_HELPER_IOURING_GET_SQE 1019

static long (*io_uring_init_global)(void) = (void *)EXTENDED_HELPER_IOURING_INIT;
static long (*io_uring_submit_write)(int fd, const void *buf, unsigned long long size) = (void *)EXTENDED_HELPER_IOURING_SUBMIT_WRITE;
static long (*io_uring_submit_read)(int fd, char *buf, unsigned long long size, unsigned long long offset) = (void *)EXTENDED_HELPER_IOURING_SUBMIT_READ;
static long (*io_uring_submit_send)(int fd, const void *buf, unsigned long long size, int flags) = (void *)EXTENDED_HELPER_IOURING_SUBMIT_SEND;
static long (*io_uring_submit_recv)(int fd, void *buf, unsigned long long size, int flags) = (void *)EXTENDED_HELPER_IOURING_SUBMIT_RECV;
static long (*io_uring_submit_fsync)(int fd) = (void *)EXTENDED_HELPER_IOURING_SUBMIT_FSYNC;
static long (*io_uring_wait_and_seen)(void) = (void *)EXTENDED_HELPER_IOURING_WAIT_AND_SEEN;
static long (*io_uring_usubmit)(void) = (void *)EXTENDED_HELPER_IOURING_SUBMIT;
static long (*io_uring_usubmit_and_wait)(unsigned int min_complete) = (void *)EXTENDED_HELPER_IOURING_SUBMIT_AND_WAIT;
static long (*io_uring_submit_pread)(int fd, void *buf, unsigned long long size, unsigned long long offset) = (void *)EXTENDED_HELPER_IOURING_SUBMIT_PREAD;
static long (*io_uring_set_linkflag)(void) = (void *)EXTENDED_HELPER_IOURING_SET_LINK;
static long (*io_uring_wait)(void) = (void *)EXTENDED_HELPER_IOURING_WAIT;
static long (*io_uring_seen)(void) = (void *)EXTENDED_HELPER_IOURING_SEEN;
static long (*io_uring_get_sqe)(void) = (void *)EXTENDED_HELPER_IOURING_GET_SQE;

#define BATCH_SIZE 1024

struct pread_submit_info {
    int pid;
    int fd;
    char *dest;
    size_t size;
    u64 i_ino;
    u32 seq_num;
    u32 depends_on;
    u_int res;
};

struct fd_info {
    size_t total_bytes_to_read;
    size_t batched_preads;
    int fd;
};

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __type(key, u32);
    __type(value, struct pread_submit_info);
    __uint(max_entries, 10000);
    __uint(map_flags, 0);
} pread_submit_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __type(key, int);
    __type(value, struct fd_info);
    __uint(max_entries, 1000);
    __uint(map_flags, 0);
} fd_info_map SEC(".maps");

static __always_inline int handle_batched_ops() {
    io_uring_usubmit();
    for (int i = 0; i < BATCH_SIZE; i++) {
        io_uring_wait_and_seen();
    }
    return 0;
}

static __always_inline int add_pending_op(struct pread_submit_info *info) {
    int res = io_uring_submit_pread(info->fd, info->dest, info->size, 0);
    info->res = res;
    bpf_map_update_elem(&pread_submit_map, &info->seq_num, info, BPF_ANY);
    return res;
}

SEC("usdt/clickhouse/main_entry")
int main_entry(struct pt_regs *ctx) {
    io_uring_init_global();
    global = true;
    return 0;
}

SEC("usdt/clickhouse/readS")
int readS(struct pt_regs *ctx) {
    const char *self_type;
    size_t n;
    char *to;
    bool *is_overridden;
    size_t *bytes_copied;

    self_type = (const char *)PT_REGS_PARM1(ctx);
    n = PT_REGS_PARM2(ctx);
    to = (char *)PT_REGS_PARM3(ctx);
    is_overridden = (bool *)PT_REGS_PARM4(ctx);
    bytes_copied = (size_t *)PT_REGS_PARM5(ctx);

    bool overridden = true;
    bpf_probe_write_user(is_overridden, &overridden, sizeof(overridden));

    u32 pid = bpf_get_current_pid_tgid() >> 32;
    struct pread_submit_info info = {0};
    info.pid = pid;
    info.dest = to;
    info.size = n;

    bpf_map_update_elem(&pread_submit_map, &pid, &info, BPF_ANY);

    return 0;
}

SEC("usdt/clickhouse/preadRaw")
int preadRaw(struct pt_regs *ctx) {
    int fd;
    size_t offset;
    ssize_t *res;
    bool *bpf_override;
    size_t to_read;

    fd = PT_REGS_PARM1(ctx);
    offset = PT_REGS_PARM2(ctx);
    res = (ssize_t *)PT_REGS_PARM3(ctx);
    bpf_override = (bool *)PT_REGS_PARM4(ctx);
    to_read = PT_REGS_PARM5(ctx);

    bool overridden = true;
    bpf_probe_write_user(bpf_override, &overridden, sizeof(overridden));

    u32 pid = bpf_get_current_pid_tgid() >> 32;
    struct pread_submit_info *info;
    info = bpf_map_lookup_elem(&pread_submit_map, &pid);
    
    if (info) {
        info->fd = fd;
        info->size = to_read;
        info->i_ino = -1;
        info->seq_num = global_seq++; 
        info->depends_on = 0;

        add_pending_op(info);

        struct fd_info *fd_info = bpf_map_lookup_elem(&fd_info_map, &fd);
        if (fd_info) {
            fd_info->total_bytes_to_read += to_read;
            fd_info->batched_preads++;
        } else {
            struct fd_info new_fd_info = {
                .total_bytes_to_read = to_read,
                .batched_preads = 1,
                .fd = fd
            };
            u32 pid = bpf_get_current_pid_tgid() >> 32;
            bpf_map_update_elem(&fd_info_map, &pid, &new_fd_info, BPF_ANY);
        }
    }

    return 0;
}
 
SEC("usdt/clickhouse/batchSubmit")
int batchSubmit(struct pt_regs *ctx) {
    size_t *bytes_read = (size_t *)PT_REGS_PARM1(ctx);
    u32 pid = bpf_get_current_pid_tgid() >> 32;
    struct fd_info *fd_info = bpf_map_lookup_elem(&fd_info_map, &pid);
    if (fd_info == NULL) return 0;
    
    bpf_probe_write_user(bytes_read, &fd_info->total_bytes_to_read, sizeof(size_t));

    handle_batched_ops();
    return 0;
}

SEC("usdt/clickhouse/readCount")
int readCount(struct pt_regs *ctx) {
    u64 duration;
    duration = PT_REGS_PARM1(ctx);

    bpf_printk("Read latency: %lld\n", duration);
    
    return 0;
}

char LICENSE[] SEC("license") = "GPL";