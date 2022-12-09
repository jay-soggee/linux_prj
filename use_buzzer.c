#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

int main(int argc, char **argv){
    char buff;

    if(argc < 2){
        printf("Put arg a, b, c, d or e\n");
        return -1;
    }

    int dev = open("/dev/my_buzzer_driver",O_RDWR);
    if(dev == -1){
        printf("Opening was not possible!\n");
        return -1;
    }
    printf("Opening was successful!\n");

    printf("input : %c\n", *argv[1]);
    write(dev, argv[1], 1);


    return 0;

}