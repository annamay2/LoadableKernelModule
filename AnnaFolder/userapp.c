#include <stdio.h>
#include <fcntl.h>      // For open()
#include <unistd.h>     // For close() and read()
#include <errno.h>      // For error handling
#include <sys/ioctl.h>  // For ioctl() system call
#include <string.h>     // For strstr() and strtok()

// Define the device file path corresponding to the mouse logger kernel module
#define DEVICE_FILE "/dev/mouse_logger_1"

// Define the ioctl command to clear the event buffer in the kernel module
#define MOUSE_LOGGER_CLEAR _IO('M', 1)

int main() {
    char buffer[256];  // Buffer to store read data from the device file
    int fd = open(DEVICE_FILE, O_RDONLY); // Open the device file in read-only mode

    // Check if the device file was opened successfully
    if (fd < 0) {
        perror("Failed to open device file");
        return 1;
    }

    // Send an ioctl command to clear the buffer before reading new events
    if (ioctl(fd, MOUSE_LOGGER_CLEAR) < 0) {
        perror("Failed to clear buffer");
        close(fd);
        return 1;
    }

    printf("Listening for mouse clicks...\n");

    while (1) {
        // Read data from the device file into the buffer
        ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);

        // Check for read errors
        if (bytes_read < 0) {
            perror("Read failed");
            break;
        } else if (bytes_read == 0) {
            // If read returns 0, it means there is no data available
            // Normally, this should not happen unless the device is non-blocking
            printf("No data available, but read returned 0. Is the device non-blocking?\n");
            break;
        }

        // Null-terminate the buffer to ensure it can be treated as a string
        buffer[bytes_read] = '\0';

        // Process the buffer line by line using strtok() function
        char *line = buffer;
        while ((line = strtok(line, "\n")) != NULL) {
            // Check if the line contains a mouse click event ("Click" keyword)
            if (strstr(line, "Click") != NULL) {
                printf("Mouse Event: %s\n", line);
            }
            line = NULL; // Set to NULL to continue tokenizing the buffer
        }
    }

    // Close the device file before exiting
    close(fd);
    return 0;
}
