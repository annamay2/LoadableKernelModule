#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#define DEVICE_FILE "/dev/mouse_logger_1"

int main() {
    char buffer[256];
    int fd = open(DEVICE_FILE, O_RDONLY);

    if (fd < 0) {
        perror("Failed to open device file");
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

        // Null-terminate the buffer and print the mouse event
        buffer[bytes_read] = '\0';
        printf("Mouse Event: %s", buffer);
    }

    close(fd);
    return 0;
}
