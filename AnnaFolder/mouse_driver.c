#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/input.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/sched.h>  // For wait queues
#include <linux/wait.h>   // For wait queues
#include <linux/ioctl.h>  // For ioctl support

#define DEVICE_NAME "mouse_logger_1"
#define BUFFER_SIZE 256

// Define ioctl command
#define MOUSE_LOGGER_CLEAR _IO('M', 1)

static int major_number;
static struct cdev mouse_cdev;
static struct class *mouse_class;
static struct input_handler mouse_handler;
static struct input_dev *input_device;
static char event_buffer[BUFFER_SIZE];
static int buffer_pos = 0;
static DEFINE_MUTEX(buffer_lock);

// Wait queue for blocking read
static DECLARE_WAIT_QUEUE_HEAD(mouse_wait_queue);
static int data_available = 0; // Flag to indicate new data

// Write event to buffer
static void log_event(const char *event) {
    mutex_lock(&buffer_lock);
    if (buffer_pos + strlen(event) + 2 < BUFFER_SIZE) { // +2 for newline and null terminator
        buffer_pos += snprintf(event_buffer + buffer_pos, BUFFER_SIZE - buffer_pos, "%s\n", event);
        data_available = 1; // Set flag to indicate new data
        wake_up_interruptible(&mouse_wait_queue); // Wake up waiting processes
    } else {
        printk(KERN_WARNING "Mouse Logger: Buffer full, discarding event: %s\n", event);
    }
    mutex_unlock(&buffer_lock);

    printk(KERN_INFO "Mouse Logger: Logged event - %s\n", event);
}

// Clear the buffer
static void clear_buffer(void) {
    mutex_lock(&buffer_lock);
    buffer_pos = 0; // Reset buffer position
    data_available = 0; // Reset data available flag
    mutex_unlock(&buffer_lock);
    printk(KERN_INFO "Mouse Logger: Buffer cleared\n");
}

static ssize_t mouse_read(struct file *file, char __user *user_buffer, size_t len, loff_t *offset) {
    mutex_lock(&buffer_lock);

    // Block until data is available
    while (buffer_pos == 0) {
        mutex_unlock(&buffer_lock);

        if (file->f_flags & O_NONBLOCK) {
            return -EAGAIN; // Non-blocking mode, return immediately
        }

        if (wait_event_interruptible(mouse_wait_queue, data_available)) {
            return -ERESTARTSYS; // Interrupted by signal
        }

        mutex_lock(&buffer_lock);
    }

    // Copy data to user space
    size_t bytes_to_copy = min(len, (size_t)buffer_pos);
    if (copy_to_user(user_buffer, event_buffer, bytes_to_copy)) {
        mutex_unlock(&buffer_lock);
        return -EFAULT;
    }

    // Update buffer and offset
    buffer_pos = 0; // Reset buffer after reading
    data_available = 0; // Reset data available flag
    *offset += bytes_to_copy;

    mutex_unlock(&buffer_lock);

    return bytes_to_copy;
}

// Handle ioctl commands
static long mouse_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    switch (cmd) {
        case MOUSE_LOGGER_CLEAR:
            clear_buffer();
            return 0;
        default:
            return -ENOTTY; // Unknown command
    }
}

// Open function (not much needed here)
static int mouse_open(struct inode *inode, struct file *file) {
    return 0;
}

// Release function
static int mouse_release(struct inode *inode, struct file *file) {
    return 0;
}

// File operations struct
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = mouse_read,
    .open = mouse_open,
    .release = mouse_release,
    .unlocked_ioctl = mouse_ioctl, // Add ioctl support
};

// Mouse event callback
static void mouse_event(struct input_handle *handle, unsigned int type, unsigned int code, int value) {
    printk(KERN_INFO "Mouse Logger: Received event - type: %u, code: %u, value: %d\n", type, code, value);

    if (type == EV_KEY && value) {
        if (code == BTN_LEFT) log_event("Left Click");
        else if (code == BTN_RIGHT) log_event("Right Click");
        else if (code == BTN_MIDDLE) log_event("Middle Click");
    }
}

// Connect function (called when a device is found)
static int mouse_connect(struct input_handler *handler, struct input_dev *dev, const struct input_device_id *id) {
    struct input_handle *handle;

    if (!dev) {
        printk(KERN_ERR "Mouse Logger: NULL device detected in connect()\n");
        return -ENODEV;
    }

    // Ensure the device has mouse buttons before attaching
    if (!test_bit(EV_KEY, dev->evbit) || !test_bit(BTN_LEFT, dev->keybit)) {
        printk(KERN_INFO "Mouse Logger: Skipping non-mouse device: %s\n", dev->name);
        return -ENODEV;
    }

    handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
    if (!handle) return -ENOMEM;

    handle->dev = dev;
    handle->handler = handler;
    handle->name = "mouse_logger";

    if (input_register_handle(handle)) {
        printk(KERN_ERR "Mouse Logger: Failed to register handle\n");
        kfree(handle);
        return -EINVAL;
    }

    if (input_open_device(handle)) {
        printk(KERN_ERR "Mouse Logger: Failed to open device\n");
        input_unregister_handle(handle);
        kfree(handle);
        return -EINVAL;
    }

    printk(KERN_INFO "Mouse Logger: Connected to device %s\n", dev->name);
    return 0;
}

static void mouse_disconnect(struct input_handle *handle) {
    printk(KERN_INFO "Mouse Logger: Device disconnected: %s\n", handle->dev->name);
    input_close_device(handle);
    input_unregister_handle(handle);
    kfree(handle);
}

// Input handler setup
static const struct input_device_id mouse_ids[] = {
    { .driver_info = 1 },  // Matches all devices
    { },
};

MODULE_DEVICE_TABLE(input, mouse_ids);

// Input device registration
static struct input_handler mouse_handler = {
    .event      = mouse_event,
    .connect    = mouse_connect,
    .disconnect = mouse_disconnect,
    .name       = "mouse_logger_handler",
    .id_table   = mouse_ids,
};

// Module initialization
static int __init mouse_init(void) {
    dev_t dev;

    // Allocate major number
    if (alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME) < 0) return -1;
    major_number = MAJOR(dev);

    // Register character device
    cdev_init(&mouse_cdev, &fops);
    if (cdev_add(&mouse_cdev, dev, 1) < 0) return -1;

    // Create class and device entry in /dev/
    mouse_class = class_create(DEVICE_NAME);

    if (IS_ERR(mouse_class)) {
        unregister_chrdev_region(dev, 1);
        return PTR_ERR(mouse_class);
    }

    device_create(mouse_class, NULL, dev, NULL, DEVICE_NAME);

    printk(KERN_INFO "Mouse Logger: Registering input handler...\n");

    if (input_register_handler(&mouse_handler)) {
        printk(KERN_ERR "Mouse Logger: Failed to register input handler\n");
        class_destroy(mouse_class);
        unregister_chrdev_region(dev, 1);
        return -EINVAL;
    }

    printk(KERN_INFO "Mouse Logger Loaded. Use: cat /dev/%s\n", DEVICE_NAME);
    return 0;
}

// Module cleanup
static void __exit mouse_exit(void) {
    dev_t dev = MKDEV(major_number, 0);

    printk(KERN_INFO "Mouse Logger: Unregistering input handler...\n");
    input_unregister_handler(&mouse_handler);

    device_destroy(mouse_class, dev);
    class_destroy(mouse_class);
    cdev_del(&mouse_cdev);
    unregister_chrdev_region(dev, 1);

    printk(KERN_INFO "Mouse Logger Unloaded.\n");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Custom");
MODULE_DESCRIPTION("Mouse Logger using /dev Interface");

module_init(mouse_init);
module_exit(mouse_exit);
