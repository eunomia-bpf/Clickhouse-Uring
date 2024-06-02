#include "liburing.h"
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

int test() {
  printf("test for fsync\n");
  int fd = open("temp.txt", O_APPEND | O_CREAT | O_WRONLY, 0644);
  if (fd < 0) {
    perror("Failed to open file");
    return 1;
  }
  printf("fd: %d\n", fd);
  const char *data = "Hello";
  int count = 0;
  time_t start = time(NULL);

  while (time(NULL) - start < 3) {
    if (write(fd, data, 5) != 5) {
        perror("Failed to write");
        break;
    }
    // int vb = reverse_count(20);
    if (fsync(fd) < 0) {
        perror("Failed to fsync");
        break;
    }
    count++;
    // int batch_size = 240;
    // struct io_uring_cqe *cqe = 0;
    // for (int i = 0; i < batch_size; i++) {
    //   submit_io_uring_write(&ring, fd, data, 5);
    //   submit_io_uring_fsync(&ring, fd);
    // }
    // int vb = reverse_count(20);
    // io_uring_submit(&ring);
    // for (int i = 0; i < batch_size * 2; i++) {
    //   io_uring_wait_and_seen(&ring, cqe);
    // }
    // count += batch_size;
  }

  close(fd);
  printf("Number of write and fsync calls in 3 seconds: %d\n", count);

  return 0;
}

__attribute_noinline__ void start() {
  printf("start\n");
//   io_uring_init(&ring);
}

int main() {
  start();
  while (1) {
    test();
    sleep(1);
  }
  return 0;
}
