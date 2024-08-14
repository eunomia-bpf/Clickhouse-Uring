#include "liburing.h"
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

int test() {
  printf("test for read\n");
  int fd = open("temp.txt", O_RDONLY);
  if (fd < 0) {
    perror("Failed to open file");
    return 1;
  }
  printf("fd: %d\n", fd);
  char buffer[4];
  int count = 0;
  time_t start = time(NULL);

  while (time(NULL) - start < 0.4) {
    if (pread(fd, buffer, sizeof(buffer), sizeof(buffer)*count) <= 0) {
      perror("Failed to read");
      break;
    }
    count++;
  }

  close(fd);
  printf("Number of read calls in 0.4 seconds: %d\n", count);

  return 0;
}

__attribute_noinline__ void start() {
  printf("start\n");
}

int main() {
  start();
  while (1) {
    test();
    sleep(1);
  }
  return 0;
}