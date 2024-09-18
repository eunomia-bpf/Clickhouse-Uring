#include "vmlinux.h"
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/usdt.bpf.h>


int global = false;
int global_seq = 0;
bool overridden = false;

struct pread_prepare_info {
    int pid;
    int fd;
    char *dest;
    size_t size;
    u64 i_ino;
    size_t offset;
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
    __type(key, int);
    __type(value, struct fd_info);
    __uint(max_entries, 1000);
    __uint(map_flags, 0);
} fd_info_map SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1024);
} pread_prepare_ringbuf SEC(".maps");

struct {
    __uint(type, BPF_MAP_TYPE_RINGBUF);
    __uint(max_entries, 1024);
} pread_submit_batch_ringbuf SEC(".maps");

static __always_inline int handle_batched_ops() {
    u32 pid = bpf_get_current_pid_tgid() >> 32;
    bpf_ringbuf_output(&pread_submit_batch_ringbuf, &pid, sizeof(pid), 0);
    return 0;
}

static __always_inline int add_pending_op(struct pread_prepare_info *info) {
    bpf_ringbuf_output(&pread_prepare_ringbuf, info, sizeof(*info), 0);
    return 0;
}

#define MAX_STR_LEN 128

static __inline int bpf_strcmp(const char *s1, const char *s2) {
    #pragma clang loop unroll(full)
    for (int i = 0; i < MAX_STR_LEN; i++) {
        if (s1[i] != s2[i]) {
            return s1[i] - s2[i];
        }
        if (s1[i] == '\0') {
            return 0;
        }
    }
    return 0;
}

SEC("usdt/clickhouse/readS")
int readS(struct pt_regs *ctx) {
    const char *self_type;
    size_t n;
    char *to;
    bool *is_overridden;
    size_t *bytes_copied;
    char target_type[] = "N2DB28ReadBufferFromFileDescriptorE";

    self_type = (const char *)PT_REGS_PARM1(ctx);
    n = PT_REGS_PARM2(ctx);
    to = (char *)PT_REGS_PARM3(ctx);
    is_overridden = (bool *)PT_REGS_PARM4(ctx);
    if (self_type == target_type) {
        overridden = true;
        bpf_probe_write_user(is_overridden, &overridden, sizeof(overridden));
    } else {
        overridden = false;
        bpf_probe_write_user(is_overridden, &overridden, sizeof(overridden));
    }

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

    if (!overridden) {
        return 0;
    }
    bpf_probe_write_user(bpf_override, &overridden, sizeof(overridden));

    u32 pid = bpf_get_current_pid_tgid() >> 32;

    struct {
        u32 pid;
        int fd;
        char *dest;
        size_t size;
        u64 i_ino;
        size_t offset;
        u32 seq_num;
        u32 depends_on;
        u_int res;
    } info = {};

    info.pid = pid;
    info.fd = fd;
    info.dest = (char *)offset;
    info.size = to_read;
    info.i_ino = -1; 
    info.seq_num = global_seq++;
    info.depends_on = 0;
    info.res = 0;

    bpf_ringbuf_output(&pread_prepare_ringbuf, &info, sizeof(info), 0);

    struct fd_info *fd_info = bpf_map_lookup_elem(&fd_info_map, &pid);
    if (fd_info) {
        fd_info->total_bytes_to_read += to_read;
        fd_info->batched_preads++;
        fd_info->fd = fd;
    } else {
        struct fd_info new_fd_info = {};

        new_fd_info.total_bytes_to_read = to_read;
        new_fd_info.batched_preads = 1;
        new_fd_info.fd = fd;

        bpf_map_update_elem(&fd_info_map, &pid, &new_fd_info, BPF_ANY);
    }

    return 0;
}

SEC("usdt/clickhouse/batchSubmit")
int batchSubmit(struct pt_regs *ctx) {
    size_t *bytes_read = (size_t *)PT_REGS_PARM1(ctx);
    u32 pid = bpf_get_current_pid_tgid() >> 32;
 
    struct fd_info *fd_info = bpf_map_lookup_elem(&fd_info_map, &pid);
    if (fd_info == NULL) {
        // bpf_printk("fd_info is NULL\n");
        return 0;
    }
    if (overridden) {
        bpf_probe_write_user(bytes_read, &fd_info->total_bytes_to_read, sizeof(size_t));
        handle_batched_ops();
    }

    overridden = false;
    return 0;
}

char LICENSE[] SEC("license") = "GPL";
