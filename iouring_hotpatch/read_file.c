// simple_read_file.c
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#define BUFFER_SIZE 4096
#define FILE_NAME "1gb.txt"

void handle_error(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main() {
    int fd;
    ssize_t bytes_read;
    char buffer[BUFFER_SIZE];

    fd = open(FILE_NAME, O_RDONLY);
    if (fd < 0) handle_error("Failed to open file");

    while ((bytes_read = read(fd, buffer, BUFFER_SIZE)) > 0) {
    }

    if (bytes_read < 0) {
        handle_error("Failed to read file");
    }

    close(fd);
    return 0;
}
