# Linux Kernel Module Project

## Build Instructions
1. Open a terminal and navigate to this directory.
2. Run:
   ```sh
   make

Load the module--
sudo insmod my_driver.ko

Create a device file--
sudo mknod /dev/my_char_device c <major_number> 0
(the major number is 240)

sudo chmod 666 /dev/my_char_device

runnthe userspace application--
./user_app

unload the module when your done--
sudo rmmod my_driver
