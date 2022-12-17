obj-m += driver_buzzer.o
obj-m += driver_gpio.o
KDIR = ~/workspace/kernel
CCC = gcc

RESULT = dbg
SRC = $(RESULT).c

all :
	make -C $(KDIR) M=$(PWD) modules 
	$(CCC) -o -lrt $(RESULT) $(SRC)

clean:
	make -C $(KDIR) M=$(PWD) clean 
	rm -f $(RESULT)