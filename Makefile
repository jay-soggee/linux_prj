obj-m += driver_buzzer.o
KDIR = ~/working/kernel
CCC = gcc

RESULT = use_buzzer
SRC = $(RESULT).c

all :
	make -C $(KDIR) M=$(PWD) modules 
	CCC -o $(RESULT) $(SRC)

clean:
	make -C $(KDIR) M=$(PWD) clean 
	rm -f $(RESULT)