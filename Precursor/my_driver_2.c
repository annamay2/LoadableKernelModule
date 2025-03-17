#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/ioctl.h>

#define DEVICE_NAME "my_char_device"
#define BUFFER_SIZE 1024

#define MY_IOCTL_MAGIC 'M'
#define IOCTL_GET_STATS _IOR(MY_IOCTL_MAGIC, 1, struct device_stats)

struct device_stats {
    int read_count;
    int write_count;
};

static struct device_stats stats = {0, 0};  // Initialize read/write counts to 0


static int major_number;
static char device_buffer[BUFFER_SIZE];
static int buffer_size = 0;
static wait_queue_head_t queue;
static struct file_operations fops;

// Function prototypes
static int my_open(struct inode *inode, struct file *file);
static int my_close(struct inode *inode, struct file *file);
static ssize_t my_read(struct file *file, char __user *user_buf, size_t size, loff_t *offset);
static ssize_t my_write(struct file *file, const char __user *user_buf, size_t size, loff_t *offset);
static long my_ioctl(struct file *file, unsigned int cmd, unsigned long arg);


// File operations structure
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = my_open,
    .release = my_close,
    .read = my_read,
    .write = my_write,
    .unlocked_ioctl = my_ioctl,
};

// Open function
static int my_open(struct inode *inode, struct file *file) {
    printk(KERN_INFO "Device opened\n");
    return 0;
}

// Close function
static int my_close(struct inode *inode, struct file *file) {
    printk(KERN_INFO "Device closed\n");
    return 0;
}

// Read function
static ssize_t my_read(struct file *file, char __user *user_buf, size_t size, loff_t *offset) {
    if (buffer_size == 0) {
        wait_event_interruptible(queue, buffer_size > 0);
    }

    size = min(size, (size_t)buffer_size);
    if (copy_to_user(user_buf, device_buffer, size)) {
        return -EFAULT;
    }

    buffer_size = 0;
    stats.read_count++;  // Increment read count

    return size;
}

// Write function (blocks if buffer is full)
static ssize_t my_write(struct file *file, const char __user *user_buf, size_t size, loff_t *offset) {
    if (buffer_size > 0) {
        wait_event_interruptible(queue, buffer_size == 0);
    }

    size = min(size, (size_t)BUFFER_SIZE);
    if (copy_from_user(device_buffer, user_buf, size)) {
        return -EFAULT;
    }

    buffer_size = size;
    wake_up_interruptible(&queue);
    stats.write_count++;  // Increment write count

    return size;
}

// IOCTL function
static long my_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    switch (cmd) {
        case IOCTL_GET_STATS:
            if (copy_to_user((struct device_stats __user *)arg, &stats, sizeof(stats)))
                return -EFAULT;
            break;

        default:
            return -EINVAL;  // Invalid IOCTL command
    }
    return 0;
}


// Module initialization
static int __init my_init(void) {
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        printk(KERN_ALERT "Failed to register character device\n");
        return major_number;
    }

    init_waitqueue_head(&queue);
    printk(KERN_INFO "Registered device with major number %d\n", major_number);
    return 0;
}

// Module exit
static void __exit my_exit(void) {
    unregister_chrdev(major_number, DEVICE_NAME);
    printk(KERN_INFO "Device unregistered\n");
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("A simple Linux kernel module with blocking read/write.");
