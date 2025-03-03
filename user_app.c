#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>

#define DEVICE_PATH "/dev/my_char_device"
#define BUFFER_SIZE 1024

volatile int running = 1;

void stop_running(int sig) {
    running = 0;
}

void *reader_thread(void *arg) {
    int fd = open(DEVICE_PATH, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open device");
        return NULL;
    }

    char buffer[BUFFER_SIZE];
    while (running) {
        ssize_t bytes_read = read(fd, buffer, BUFFER_SIZE);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';  // Null-terminate the buffer
            printf("Read from device: %s\n", buffer);
        } else {
            printf("No data available, reader is blocking...\n");
        }
        sleep(1);
    }

    close(fd);
    return NULL;
}

void *writer_thread(void *arg) {
    int fd = open(DEVICE_PATH, O_WRONLY);
    if (fd < 0) {
        perror("Failed to open device");
        return NULL;
    }

    while (running) {
        char *msg = "Hello from user space!";
        ssize_t bytes_written = write(fd, msg, strlen(msg));
        if (bytes_written < 0) {
            perror("Failed to write to device");
        } else {
            printf("Wrote to device: %s\n", msg);
        }
        sleep(2);
    }

    close(fd);
    return NULL;
}

int main() {
    signal(SIGINT, stop_running);  // Handle SIGINT to stop threads
    pthread_t reader, writer;
    pthread_create(&reader, NULL, reader_thread, NULL);
    pthread_create(&writer, NULL, writer_thread, NULL);

    pthread_join(reader, NULL);
    pthread_join(writer, NULL);

    return 0;
}
