obj-m += my_driver.o
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	gcc -o user_app user_app.c -lpthread

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f user_app
