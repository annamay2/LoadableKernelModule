#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h> // For strstr

#define DEVICE_FILE "/dev/mouse_logger_1"
#define MOUSE_LOGGER_CLEAR _IO('M', 1)

int main() {
    char buffer[256];
    int fd = open(DEVICE_FILE, O_RDONLY);

    if (fd < 0) {
        perror("Failed to open device file");
        return 1;
    }

    // Clear the buffer when the application starts
    if (ioctl(fd, MOUSE_LOGGER_CLEAR) < 0) {
        perror("Failed to clear buffer");
        close(fd);
        return 1;
    }

    printf("Listening for mouse clicks...\n");

    while (1) {
        ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);

        if (bytes_read < 0) {
            perror("Read failed");
            break;
        } else if (bytes_read == 0) {
            // No data available, but read should block unless the device is non-blocking
            printf("No data available, but read returned 0. Is the device non-blocking?\n");
            break;
        }

        // Null-terminate the buffer
        buffer[bytes_read] = '\0';

        // Process the buffer line by line
        char *line = buffer;
        while ((line = strtok(line, "\n")) != NULL) {
            // Check if the line contains a mouse click event
            if (strstr(line, "Click") != NULL) {
                printf("Mouse Event: %s\n", line);
            }
            line = NULL; // Continue tokenizing the buffer
        }
    }

    close(fd);
    return 0;
}
