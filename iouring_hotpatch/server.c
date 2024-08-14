#include "liburing.h"
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 8080
#define BUF_SIZE 1024
int count=0;

int test() {
    int listen_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addrlen = sizeof(client_addr);
    char buf[BUF_SIZE];
    ssize_t numRead;

    // Create socket
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Set up server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind socket to address
    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(listen_fd, 5) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    // Accept client connection
    client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_addrlen);
    if (client_fd < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    printf("Client connected\n");

    // // Echo loop
    // for (;;) {
    //     numRead = recv(client_fd, buf, BUF_SIZE, 0);
    //     if (numRead <= 0) {
    //         // Exit loop on EOF or error
    //         break;
    //     }
    //     if (send(client_fd, buf, numRead, 0) != numRead) {
    //         perror("send");
    //         break;
    //     }
    // }

    // close(client_fd);
    // close(listen_fd);
    // printf("Number of write and fsync calls in 3 seconds: %d\n", ++count);
    int bytesRead, response_fd, recv_fd;

    
    //create response file
    printf("Waiting for response file to be created");
    recv_fd = open("response.txt", O_RDWR);

    bytesRead = read(client_fd, buf, BUF_SIZE);
    while (bytesRead > 0) {
        write(recv_fd, buf, bytesRead);
        bytesRead = read(client_fd, buf, BUF_SIZE);
    }

    close(recv_fd);

    response_fd = open("response.txt", O_RDONLY);
    if (response_fd < 0) {
      perror("Fail to open response.txt");
      return -1;
    }

    int bytesSent = read(response_fd, buf, BUF_SIZE);
    while (bytesSent > 0) {
      send(client_fd, buf, bytesSent, 0);
      bytesSent = read(response_fd, buf, BUF_SIZE);
    }

    printf("Response sent");
    close(client_fd);
    close(response_fd);
    close(listen_fd);
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
