# Specifies that the kernel module object file should be built
obj-m := mouse_driver.o

# Kernel build directory (retrieves the correct directory for the running kernel)
KDIR := /lib/modules/$(shell uname -r)/build

# Compiler for user-space program
CC := gcc

# Compiler flags for the user-space program:
# -Wall: Enable common warnings
# -Wextra: Enable additional warnings
# -O2: Optimize the generated code for better performance
CFLAGS := -Wall -Wextra -O2

# Name of the user-space executable
TARGET := userapp

# Default rule: build both the kernel module and user program
all: kernel user

# Rule to build the kernel module
kernel:
	# Use the kernel build system to compile the module
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# Rule to build the user-space program
user: userapp.c
	# Compile userapp.c into an executable named $(TARGET)
	$(CC) $(CFLAGS) userapp.c -o $(TARGET)

# Clean rule: remove generated files
clean:
	# Use the kernel build system to clean up the module files
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	# Remove the compiled user-space application
	rm -f $(TARGET)
