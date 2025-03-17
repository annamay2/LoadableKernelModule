Compile by running make

Insert module with sudo insmod mouse_driver.ko

Check kernel messages with sudo dmesg | tail -n 20

Run sudo ./userapp for mouse clicks

Run sudo cat /dev/mouse_logger_1 to see character device file

Run cat /proc/mouse_events to see proc file

Left and right click to see output
