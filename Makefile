obj-m += driver_buzzer.o
obj-m += driver_gpio.o
obj-m += driver_fnd.o
obj-m += driver_motor.o
KDIR = ~/workspace/kernel
CCC = g++

RESULT = dbg
SRC = $(RESULT).cpp

all :
	make -C $(KDIR) M=$(PWD) modules 
	$(CCC) -o $(RESULT) $(SRC) -lrt -lSRC

clean:
	make -C $(KDIR) M=$(PWD) clean 
	rm -f $(RESULT)