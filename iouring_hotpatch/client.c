#include "liburing.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define PORT 8080
#define BUF_SIZE 1024
int count = 0;
int test() {
  int sock_fd;
  struct sockaddr_in server_addr;
  char buf[BUF_SIZE];
  ssize_t numRead;

  // Create socket
  sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_fd < 0) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  // Set up server address
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(PORT);
  if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
    perror("inet_pton");
    exit(EXIT_FAILURE);
  }

  // Connect to server
  if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    perror("connect");
    exit(EXIT_FAILURE);
  }

  printf("Connected to server\n");

  // // Read input from user and send to server
  // while (fgets(buf, BUF_SIZE, stdin) != NULL) {
  //   if (send(sock_fd, buf, strlen(buf), 0) < 0) {
  //     perror("send");
  //     break;
  //   }

  //   // Receive echoed message from server
  //   numRead = recv(sock_fd, buf, BUF_SIZE, 0);
  //   if (numRead < 0) {
  //     perror("recv");
  //     break;
  //   }

  //   // Print received message
  //   printf("Server echoed: %.*s", (int)numRead, buf);
  // }

  // close(sock_fd);
  // printf("Number of write and fsync calls in 3 seconds: %d\n", count++);
  int file_fd = open("temp.txt", O_RDONLY);
  if (file_fd < 0) {
      perror("Failed to open file temp.txt");
      return -1;
  }


  int bytes_read = read(file_fd, buf, BUF_SIZE);
  while (bytes_read > 0) {
    send(sock_fd, buf, bytes_read, 0);
    bytes_read = read(file_fd, buf, BUF_SIZE);
  }
  shutdown(sock_fd, SHUT_WR); // Signal end of data sending

  printf("Data sent\n");

  int recv_fd = open("recv.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (recv_fd < 0) {
    perror("Failed to open file recv.txt");
    close(sock_fd);
    return -1;
  }

  // Set timeout
  struct timeval tv;
  tv.tv_sec = 5;  
  tv.tv_usec = 0;
  setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

  int bytesRead;
  // Receive response from server
  while ((bytesRead = recv(sock_fd, buf, BUF_SIZE, 0)) > 0) {
    if (write(recv_fd, buf, bytesRead) != bytesRead) {
      perror("write");
      close(recv_fd);
      close(sock_fd);
      return -1;
    }
  }

  if (bytesRead < 0) {
    perror("recv");
  }

  close(recv_fd);
  close(sock_fd);

  printf("Data transfer complete\n");
  return 0;
}

__attribute_noinline__ void start() {
  printf("start\n");
  //   io_uring_init(&ring);
}

int main() {
  start();
  // while (1) {
  //   test();
  //   sleep(1);
  // }
  test();
  return 0;
}
