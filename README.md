# Linux Kernel Module Project

## Build Instructions
1. Open a terminal and navigate to this directory.
2. Run:
   ```sh
   make

Load the module--
sudo insmod my_driver.ko

Creatw a device file:
sudo mknod /dev/my_char_device c <major_number> 0
sudo chmod 666 /dev/my_char_device
(the major number is 240)
runnthe userspace application: ./user_app

unload the module when your done:
sudo rmmod my_driver
