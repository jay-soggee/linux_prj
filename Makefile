obj-m += driver_buzzer.o
obj-m += driver_gpio.o
obj-m += driver_fnd.o
obj-m += driver_motor.o
KDIR = ~/workspace/kernel
CCC = g++
INCLUDE_PATH = ~/sdm/src/include
RESULT = dbg
SOURCE = $(RESULT).cpp

all :
	make -C $(KDIR) M=$(PWD) modules 
	$(CCC) -I $(INCLUDE_PATH) -o $(RESULT) $(SOURCE) -lrt `pkg-config opencv4 --libs --cflags`

clean:
	make -C $(KDIR) M=$(PWD) clean 
	rm -f $(RESULT)