#include "liburing.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define BATCH_SIZE 256

int test_io_uring(struct io_uring *ring) {
    printf("Test for io_uring read\n");
    int fd = open("temp.txt", O_RDONLY);
    if (fd < 0) {
        perror("Failed to open file");
        return 1;
    }
    printf("fd: %d\n", fd);

    char buffer[4];
    int count = 0;
    time_t start = time(NULL);
    int batch_count = 0;
    struct io_uring_cqe *cqe;
    while (time(NULL) - start < 0.4) {
        struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
        if (!sqe) {
            fprintf(stderr, "Could not get submission queue entry\n");
            break;
        }

        io_uring_prep_read(sqe, fd, buffer, sizeof(buffer), sizeof(buffer) * count);
        batch_count++;

        if (batch_count >= BATCH_SIZE) {
            io_uring_submit_and_wait(&ring, batch_count);
            for (int i = 0; i < batch_count; i++) {
                if (io_uring_peek_cqe(&ring, &cqe) < 0) {
                    fprintf(stderr, "Error in io_uring_peek_cqe\n");
                    return -1;
                }
                if (cqe->res < 0) fprintf(stderr, "Error in read: %s\n", strerror(-cqe->res));
                io_uring_cqe_seen(&ring, cqe);
            }
            batch_count = 0;
        }
    }

    close(fd);
    printf("Number of read calls in 0.4 seconds: %d\n", count);

    return 0;
}

__attribute__((noinline)) void start() {
    printf("Start\n");
}

int main() {
    struct io_uring ring;
    int ret = io_uring_queue_init(32, &ring, 0);
    if (ret < 0) {
        fprintf(stderr, "Unable to setup io_uring: %s\n", strerror(-ret));
        return 1;
    }

    start();
    while (1) {
        test_io_uring(&ring);
        sleep(1);
    }

    io_uring_queue_exit(&ring);
    return 0;
}
