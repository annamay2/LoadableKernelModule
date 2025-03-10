#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <string.h>
#include <signal.h>

#define DEVICE_PATH "/dev/my_char_device"
#define MY_IOCTL_MAGIC 'M'
#define IOCTL_GET_STATS _IOR('M', 1, struct device_stats)
#define BUFFER_SIZE 1024

struct device_stats {
      int read_count;
      int write_count;
};

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
    int fd;
    struct device_stats stats;

    fd = open(DEVICE_PATH, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open device");
        return -1;
    }

    if (ioctl(fd, IOCTL_GET_STATS, &stats) < 0) {
        perror("IOCTL failed");
        close(fd);
        return -1;
    }

    printf("Device Statistics:\n");
    printf("Read Count: %d\n", stats.read_count);
    printf("Write Count: %d\n", stats.write_count);

    signal(SIGINT, stop_running);  // Handle SIGINT to stop threads
    pthread_t reader, writer;
    pthread_create(&reader, NULL, reader_thread, NULL);
    pthread_create(&writer, NULL, writer_thread, NULL);

    pthread_join(reader, NULL);
    pthread_join(writer, NULL);

    close(fd);
    return 0;
}
