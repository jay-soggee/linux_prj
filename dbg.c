#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#define DEBUG

typedef long int Pitime;
struct timespec gettime_now;
// get time in nanosec.
inline Pitime NOW_ns() {
    clock_gettime(CLOCK_REALTIME, &gettime_now);
    return gettime_now.tv_nsec;
}

#define DRAW    2
#define WIN     1
#define LOSE    0

int dev_svmt;
int dev_bzzr;
int dev_gpio;
int dev_fnd;

int OpenCharDev(const char* dir) {
    int tmp = open(dir, O_RDWR);
    if (tmp < 0) {
        printf("main : Opening %s is not Possible!\n", dir);
        goto Fatal;
    }
    return tmp;
}


int toggle_button_state = 0;

void buttonUpdate() {
    char            buff;
    static char     last_button_state = '0';
    static char     curr_button_state = '0';
    static Pitime   last_pushed = 0;

    read(dev_gpio, &buff, 1); // read pin 6

    if (buff != last_button_state) // if the button signal detected(pressed or noise),
        last_pushed = NOW_ns();         
    else if ((NOW_ns() - last_pushed) > msecs_to_jiffies(20)) // count the time a little
        if (buff != curr_button_state) { // if the button signal is still changed
            curr_button_state = buff;
            if (curr_button_state == '1')
                toggle_button_state = !toggle_button_state;
        }
    last_button_state = buff; // last_button_state will follow the signal(pressed or noise).
}



void playBuzzer(char song) {
    write(dev_bzzr, &song, 1);
}



int main(void) {

    //dev_svmt = OpenCharDev("/dev/my_motor_driver");
    dev_bzzr = OpenCharDev("/dev/my_buzzer_driver");
    dev_gpio = OpenCharDev("/dev/my_gpio_driver");
    //dev_fnd  = OpenCharDev("/dev/my_fnd_driver");
    printf("main : Opening char devs success...!!!\n");


    // wait for the start button pressed (behave as toggle)
    do {buttonUpdate();} 
    while (!toggle_button_state);

    playBuzzer('a');

    close(dev_bzzr);
    close(dev_gpio);
    return 0;
Fatal:
    return -1;
}