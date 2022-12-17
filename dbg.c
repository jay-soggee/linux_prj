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

const char* cdev_dirs[4] = {
    "/dev/my_motor_driver",
    "/dev/my_buzzer_driver",
    "/dev/my_gpio_driver",
    "/dev/my_fnd_driver"
};
int dev_svmt;
int dev_bzzr;
int dev_gpio;
int dev_fnd;

int openAllDev() {
    int* cdevs[4];
    cdevs[0] = &dev_svmt;
    cdevs[1] = &dev_bzzr;
    cdevs[2] = &dev_gpio;
    cdevs[3] = &dev_fnd;
    int err = 0;

    for (int i = 0; i < 4; i++) {
        cdevs[i]* = open(cdev_dirs[i], O_RDWR);
        if (cdevs[i]* < 0) {
            printf("main : Opening %s is not Possible!\n", cdev_dirs[i]);
            err -= 1;
        }
    }

    return err;
}

void closeAllDev() {
    int* cdevs[4];
    cdevs[0] = &dev_svmt;
    cdevs[1] = &dev_bzzr;
    cdevs[2] = &dev_gpio;
    cdevs[3] = &dev_fnd;
    for (int i = 0; i < 4; i++) 
        if (cdevs[i]* > 0)
            close(cdevs[i]*);
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
    else if ((NOW_ns() - last_pushed) > 20000L) // count the time a little
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

    openAllDev();

    // wait for the start button pressed (behave as toggle)
    do {buttonUpdate();} 
    while (!toggle_button_state);

    playBuzzer('a');

    // game started. wait a sec...
    Pitime time_ref = NOW_ns();
    while ((time_ref + 2000000000) > NOW_ns());

    playBuzzer('b');

    closeAllDev();
    return 0;
    
}