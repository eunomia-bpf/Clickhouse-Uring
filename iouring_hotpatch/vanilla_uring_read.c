// vanilla_iouring.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <liburing.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>

#define FILE_NAME "1gb.txt"
#define BUFFER_SIZE 4096
#define QUEUE_DEPTH 512
#define BATCH_SIZE 256

void handle_error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void benchmark_io_uring() {
    struct io_uring ring;
    struct io_uring_sqe *sqe;
    struct io_uring_cqe *cqe;
    int fd, ret;
    char buffer[BUFFER_SIZE];
    off_t file_offset = 0;
    struct stat file_stat;
    int batch_count = 0;

    fd = open(FILE_NAME, O_RDONLY);
    if (fd < 0) handle_error("Failed to open file");

    if (fstat(fd, &file_stat) < 0) handle_error("Failed to get file stats");

    if (io_uring_queue_init(QUEUE_DEPTH, &ring, 0) < 0) handle_error("io_uring initialization failed");

    time_t start = time(NULL);
    int total_reads = 0;

    while (file_offset < file_stat.st_size) {
        if (batch_count >= BATCH_SIZE) {
            io_uring_submit_and_wait(&ring, batch_count);
            for (int i = 0; i < batch_count; i++) {
                ret = io_uring_wait_cqe(&ring, &cqe);
                if (ret < 0) handle_error("Error waiting for completion");
                if (cqe->res < 0) handle_error("Read failed");
                io_uring_cqe_seen(&ring, cqe);
            }
            batch_count = 0;
        }

        sqe = io_uring_get_sqe(&ring);
        if (!sqe) handle_error("Failed to get submission queue entry");

        size_t bytes_to_read = BUFFER_SIZE;
        if (file_offset + BUFFER_SIZE > file_stat.st_size) {
            bytes_to_read = file_stat.st_size - file_offset;
        }

        io_uring_prep_read(sqe, fd, buffer, bytes_to_read, file_offset);
        sqe->user_data = 1;  

        file_offset += bytes_to_read;
        batch_count++;
        total_reads++;
    }

    if (batch_count > 0) {
        io_uring_submit_and_wait(&ring, batch_count);
        for (int i = 0; i < batch_count; i++) {
            ret = io_uring_wait_cqe(&ring, &cqe);
            if (ret < 0) handle_error("Error waiting for completion");
            if (cqe->res < 0) handle_error("Read failed");
            io_uring_cqe_seen(&ring, cqe);
        }
    }

    time_t end = time(NULL);
    printf("Total read calls: %d in %ld seconds\n", total_reads, end - start);

    close(fd);
    io_uring_queue_exit(&ring);
}

int main() {
    benchmark_io_uring();
    return 0;
}
