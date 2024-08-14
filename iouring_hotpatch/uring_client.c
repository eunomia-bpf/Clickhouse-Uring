#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <liburing.h>
#include <errno.h>
#include <sys/stat.h>

#define PORT 8080
#define BUF_SIZE 4096
#define QUEUE_DEPTH 512
#define BATCH_SIZE 256

void cleanup(int sock, int file_fd, int recv_fd, struct io_uring *ring) {
    if (sock > 0) close(sock);
    if (file_fd > 0) close(file_fd);
    if (recv_fd > 0) close(recv_fd);
    io_uring_queue_exit(ring);
}

void log_debug(const char* message) {
    printf("[DEBUG] %s\n", message);
}

void log_info(const char* message) {
    printf("[INFO] %s\n", message);
}

void log_error(const char* message) {
    fprintf(stderr, "[ERROR] %s\n", message);
}

int main() {
    int sock = 0, file_fd = -1, recv_fd = -1;
    struct sockaddr_in serv_addr;
    char *buffer;
    struct io_uring ring;
    struct io_uring_sqe *sqe;
    struct io_uring_cqe *cqe;
    off_t file_offset = 0, recv_file_offset = 0;

    // Allocate aligned buffer
    if (posix_memalign((void **)&buffer, 4096, BUF_SIZE)) {
        log_error("Failed to allocate aligned buffer");
        return -1;
    }

    // Open the file to read (a.txt)
    if ((file_fd = open("1gb.txt", O_RDONLY)) < 0) {
        log_error("Failed to open file a.txt");
        free(buffer);
        return -1;
    }

    // Create and configure socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        log_error("Socket creation error");
        cleanup(sock, file_fd, recv_fd, &ring);
        free(buffer);
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        log_error("Invalid address / Address not supported");
        cleanup(sock, file_fd, recv_fd, &ring);
        free(buffer);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        log_error("Connection Failed");
        cleanup(sock, file_fd, recv_fd, &ring);
        free(buffer);
        return -1;
    }

    // Initialize io_uring with default parameters
    if (io_uring_queue_init(QUEUE_DEPTH, &ring, 0) < 0) {
        log_error("io_uring initialization failed");
        cleanup(sock, file_fd, recv_fd, &ring);
        free(buffer);
        return -1;
    }

    log_info("Starting to read file and send data to the server");

    // Read from file using io_uring and send to server
    int batch_count = 0;
    struct stat file_stat;
    fstat(file_fd, &file_stat);

    while (1) {
        if (file_offset >= file_stat.st_size) {
            log_info("End of file reached");
            break;
        }

        // Determine bytes to read for this operation
        size_t bytes_to_read = BUF_SIZE;
        if (file_offset + BUF_SIZE > file_stat.st_size) {
            bytes_to_read = file_stat.st_size - file_offset;
        }

        // Prepare and submit read operation
        sqe = io_uring_get_sqe(&ring);
        if (!sqe) {
            log_error("io_uring_get_sqe failed for read operation");
            cleanup(sock, file_fd, recv_fd, &ring);
            free(buffer);
            return -1;
        }

        io_uring_prep_read(sqe, file_fd, buffer, bytes_to_read, file_offset);
        sqe->user_data = 1;  // User data to identify read operation
        batch_count++;
        file_offset += bytes_to_read;

        // Prepare send operation
        sqe = io_uring_get_sqe(&ring);
        if (!sqe) {
            log_error("io_uring_get_sqe failed for send operation");
            cleanup(sock, file_fd, recv_fd, &ring);
            free(buffer);
            return -1;
        }

        io_uring_prep_send(sqe, sock, buffer, bytes_to_read, 0);
        sqe->user_data = 2;  // User data to identify send operation
        batch_count++;

        if (batch_count >= BATCH_SIZE) {
            io_uring_submit_and_wait(&ring, batch_count);
            for (int i = 0; i < batch_count; i++) {
                if (io_uring_peek_cqe(&ring, &cqe) < 0) {
                    log_error("Error peeking for cqe during send operation");
                    cleanup(sock, file_fd, recv_fd, &ring);
                    free(buffer);
                    return -1;
                }
                if (cqe->res < 0) log_error("Send error");
                io_uring_cqe_seen(&ring, cqe);
            }
            batch_count = 0;
        }
    }

    // Handle remaining SQEs for write operations
    if (batch_count > 0) {
        io_uring_submit_and_wait(&ring, batch_count);
        for (int i = 0; i < batch_count; i++) {
            if (io_uring_peek_cqe(&ring, &cqe) < 0) {
                log_error("Error peeking for cqe for remaining write operations");
                cleanup(sock, file_fd, recv_fd, &ring);
                free(buffer);
                return -1;
            }
            if (cqe->res < 0) log_error("Write error");
            io_uring_cqe_seen(&ring, cqe);
        }
    }

    log_info("File content sent to server");

    close(file_fd);  // File read is complete

    // Signal the end of sending data
    shutdown(sock, SHUT_WR);

    // Open the file to write the server's response (recv.txt)
    if ((recv_fd = open("recv.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) {
        log_error("Failed to open file recv.txt");
        cleanup(sock, file_fd, recv_fd, &ring);
        free(buffer);
        return -1;
    }

    log_info("Receiving response from server");

    // Receive data from server and write to file using io_uring
    while (1) {
        // Prepare and submit recv operation
        sqe = io_uring_get_sqe(&ring);
        if (!sqe) {
            log_error("io_uring_get_sqe failed for recv operation");
            cleanup(sock, file_fd, recv_fd, &ring);
            free(buffer);
            return -1;
        }

        io_uring_prep_recv(sqe, sock, buffer, BUF_SIZE, 0);
        sqe->user_data = 3;  // User data to identify recv operation
        io_uring_submit(&ring);

        // Wait for the completion of the recv operation
        if (io_uring_wait_cqe(&ring, &cqe) < 0) {
            log_error("Error waiting for cqe during recv operation");
            cleanup(sock, file_fd, recv_fd, &ring);
            free(buffer);
            return -1;
        }

        if (cqe->res <= 0) {
            if (cqe->res < 0) log_error("Receive error");
            // If the result is 0, it means the connection was closed
            io_uring_cqe_seen(&ring, cqe);
            break;
        }

        size_t bytes_received = cqe->res;
        io_uring_cqe_seen(&ring, cqe);

        // Prepare and submit write operation
        sqe = io_uring_get_sqe(&ring);
        if (!sqe) {
            log_error("io_uring_get_sqe failed for write operation");
            cleanup(sock, file_fd, recv_fd, &ring);
            free(buffer);
            return -1;
        }

        io_uring_prep_write(sqe, recv_fd, buffer, bytes_received, recv_file_offset);
        sqe->user_data = 4;  // User data to identify write operation
        io_uring_submit(&ring);

        // Wait for the completion of the write operation
        if (io_uring_wait_cqe(&ring, &cqe) < 0) {
            log_error("Error waiting for cqe during write operation");
            cleanup(sock, file_fd, recv_fd, &ring);
            free(buffer);
            return -1;
        }

        if (cqe->res < 0) log_error("Write error");
        io_uring_cqe_seen(&ring, cqe);

        recv_file_offset += bytes_received;
    }

    log_info("Response received and written to file");

    // Cleanup
    close(recv_fd);
    io_uring_queue_exit(&ring);
    close(sock);
    free(buffer);
    log_info("Operation completed successfully");
    return 0;
}
