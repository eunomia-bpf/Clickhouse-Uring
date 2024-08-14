#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <sys/sdt.h>

#define BUFFER_SIZE 1024
int main() {
  int sourceFile, destFile;
  char buffer[BUFFER_SIZE];
  int bytesRead, bytesWritten;

  sourceFile = open("./1gb.txt", O_RDONLY);
  if (sourceFile == -1) {
    perror("Error opening source file");
    return EXIT_FAILURE;
  }
  
  destFile =
      open("./temp.txt", O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  if (destFile == -1) {
    perror("Error opening destination file");
    return EXIT_FAILURE;
  }

  clock_t start = clock();
  int count = 0;

  struct stat file_stat;
  if (fstat(sourceFile, &file_stat) == -1) {
    perror("Error getting file stats");
    return -1;
  }
  int remaining_bytes = file_stat.st_size;
  printf("File size: %d\n", remaining_bytes);
  while (remaining_bytes > 0) {
    bytesRead = BUFFER_SIZE;
    if (remaining_bytes < BUFFER_SIZE) {
      bytesRead = remaining_bytes;
    }
    bytesRead = pread(sourceFile, buffer, bytesRead, count * BUFFER_SIZE);
    DTRACE_PROBE1(read, bytesRead, &bytesRead);
    // printf("Bytes read: %d\n", bytesRead);

    remaining_bytes -= bytesRead;

    bytesWritten = write(destFile, buffer, bytesRead);
    // printf("Bytes written: %d\n", bytesWritten);

    if (bytesWritten == -1) {
      perror("Error writing to destination file");
      return EXIT_FAILURE;
    }
    count++;
  }

  clock_t end = clock();

  close(destFile);
  close(sourceFile);
  printf("File copied successfully in %f seconds\n",
         (double)(end - start) / CLOCKS_PER_SEC);

  return EXIT_SUCCESS;
}
