Compile by running make
Insert module with sudo insmod mouse_driver.ko
Check kernel messages with sudo dmesg | tail -n 20
To see kernel messages update live, open a new terminal page with the button in the top left corner, and run sudo dmesg -w
Run sudo ./userapp
Left and right click to see output
