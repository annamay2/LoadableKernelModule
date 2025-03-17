#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/input.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/sched.h>  
#include <linux/wait.h>   
#include <linux/ioctl.h>  
#include <linux/proc_fs.h> 

// constants for creating dev and proc files
#define DEVICE_NAME "mouse_logger_1"
#define PROC_FILE_NAME "mouse_events"

// Declaring ioctl command to clear buffer - M is magic number
#define MOUSE_LOGGER_CLEAR _IO('M', 1)

// variables for device registration, used in init function
static int major_number;
static struct cdev mouse_cdev;
static struct class *mouse_class;
static struct input_handler mouse_handler;

// Buffer for mouse events
#define BUFFER_SIZE 256
static char event_buffer[BUFFER_SIZE];
static int buffer_pos = 0;
static DEFINE_MUTEX(buffer_lock); // Mutex makes sure user app doesn't read before driver is done writing

// Wait queue for blocking read operations - process is put to sleep if there is no data
static DECLARE_WAIT_QUEUE_HEAD(mouse_wait_queue);
static int data_available = 0; // Flag to indicate there is data to read

// stores location of proc file
static struct proc_dir_entry *proc_file;

// Function to log mouse events into the buffer
static void log_event(const char *event) {
    mutex_lock(&buffer_lock); 
    int event_len = strlen(event) + 1;  

    // FIFO - removes oldest entry when buffer is full (rarely used - buffer is cleared on every read anyways)
    while (buffer_pos + event_len >= BUFFER_SIZE) {
        // Uses the "\n" character to identify first entry
        char *oldest_event = strchr(event_buffer, '\n');
        if (oldest_event) {
            int shift_amount = (oldest_event - event_buffer) + 1; 
            memmove(event_buffer, oldest_event + 1, BUFFER_SIZE - shift_amount);
            buffer_pos -= shift_amount;
        } else {
            // If no newline is found, just clear buffer
            buffer_pos = 0;
        }
    }

    // Log new event
    buffer_pos += snprintf(event_buffer + buffer_pos, BUFFER_SIZE - buffer_pos, "%s\n", event);
    data_available = 1; 

    // Wakes up waiting read process
    wake_up_interruptible(&mouse_wait_queue); 

    mutex_unlock(&buffer_lock); 
}

// Function to clear the event buffer
static void clear_buffer(void) {
    mutex_lock(&buffer_lock);
    buffer_pos = 0; // Reset buffer position
    data_available = 0; 
    mutex_unlock(&buffer_lock);
    printk(KERN_INFO "Mouse Logger: Buffer cleared\n");
}

// used by userspace to read from device file (defined in fops)
static ssize_t proc_read(struct file *file, char __user *user_buffer, size_t len, loff_t *offset) {
    mutex_lock(&buffer_lock);

    while (buffer_pos == 0) {
        // mutex unlocks if there is no data, locks again when process wakes up
        mutex_unlock(&buffer_lock);
        if (wait_event_interruptible(mouse_wait_queue, data_available)) return -ERESTARTSYS; // Handle interruption
        mutex_lock(&buffer_lock);
    }

    // Copy event data to user space
    size_t bytes_to_copy = min(len, (size_t)buffer_pos);
    if (copy_to_user(user_buffer, event_buffer, bytes_to_copy)) {
        mutex_unlock(&buffer_lock);
        return -EFAULT;
    }

    buffer_pos = 0;
    data_available = 0;
    *offset += bytes_to_copy;

    mutex_unlock(&buffer_lock);
    return bytes_to_copy;
}

// Proc file operations - only read is used (input device)
static const struct proc_ops proc_fops = {
    .proc_read = proc_read,
};

// ioctl command to clear the buffer
static long mouse_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    switch (cmd) {
        case MOUSE_LOGGER_CLEAR:
            clear_buffer();
            return 0;
        default:
            return -ENOTTY; // Unknown command
    }
}

// User space commands (read and ioctl)
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = proc_read, // Use proc_read function for device reads
    .unlocked_ioctl = mouse_ioctl,
};

// Callback function to handle mouse events
static void mouse_event(struct input_handle *handle, unsigned int type, unsigned int code, int value) {
    char event[64];
    if (type == EV_KEY && value) {
        if (code == BTN_LEFT) log_event("Left Click");
        else if (code == BTN_RIGHT) log_event("Right Click");
        else if (code == BTN_MIDDLE) log_event("Middle Click");
    } else if (type == EV_REL) {
        if (code == REL_X) {
            snprintf(event, sizeof(event), "Mouse Move: X=%d", value);
            log_event(event);
        } else if (code == REL_Y) {
            snprintf(event, sizeof(event), "Mouse Move: Y=%d", value);
            log_event(event);
        }
    }
}

// Function to handle new mouse device connection
static int mouse_connect(struct input_handler *handler, struct input_dev *dev, const struct input_device_id *id) {
    struct input_handle *handle;
    if (!dev) return -ENODEV;
    if (!test_bit(EV_KEY, dev->evbit) || !test_bit(BTN_LEFT, dev->keybit)) return -ENODEV;

    handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
    if (!handle) return -ENOMEM;

    handle->dev = dev;
    handle->handler = handler;
    handle->name = "mouse_logger";

    if (input_register_handle(handle)) {
        kfree(handle);
        return -EINVAL;
    }
    if (input_open_device(handle)) {
        input_unregister_handle(handle);
        kfree(handle);
        return -EINVAL;
    }

    printk(KERN_INFO "Mouse Logger: Connected to device %s\n", dev->name);
    return 0;
}

static void mouse_disconnect(struct input_handle *handle) {
    input_close_device(handle);
    input_unregister_handle(handle);
    kfree(handle);
    printk(KERN_INFO "Mouse Logger: Device Disconnected\n");

}

// Input device registration
static const struct input_device_id mouse_ids[] = {
    { .driver_info = 1 },
    { },
};

// Defines which functions are called depending on the event
MODULE_DEVICE_TABLE(input, mouse_ids);
static struct input_handler mouse_handler = {
    .event      = mouse_event,
    .connect    = mouse_connect,
    .disconnect = mouse_disconnect,
    .name       = "mouse_logger_handler",
    .id_table   = mouse_ids,
};

// Module initialization function
static int __init mouse_init(void) {
    dev_t dev;
    if (alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME) < 0) return -1;
    major_number = MAJOR(dev);

    cdev_init(&mouse_cdev, &fops);
    if (cdev_add(&mouse_cdev, dev, 1) < 0) return -1;

    mouse_class = class_create(DEVICE_NAME);
    if (IS_ERR(mouse_class)) {
        unregister_chrdev_region(dev, 1);
        return PTR_ERR(mouse_class);
    }
    device_create(mouse_class, NULL, dev, NULL, DEVICE_NAME);

    proc_file = proc_create(PROC_FILE_NAME, 0, NULL, &proc_fops);
    if (!proc_file) return -ENOMEM;

    if (input_register_handler(&mouse_handler)) return -EINVAL;

    printk(KERN_INFO "Mouse Logger Loaded. Use: cat /proc/%s\n", PROC_FILE_NAME);
    return 0;
}

// Module cleanup function
static void __exit mouse_exit(void) {
    dev_t dev = MKDEV(major_number, 0);
    input_unregister_handler(&mouse_handler);
    proc_remove(proc_file);
    device_destroy(mouse_class, dev);
    class_destroy(mouse_class);
    cdev_del(&mouse_cdev);
    unregister_chrdev_region(dev, 1);
    printk(KERN_INFO "Mouse Logger Unloaded.\n");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Custom");
MODULE_DESCRIPTION("Mouse Logger using /proc Interface");
module_init(mouse_init);
module_exit(mouse_exit);