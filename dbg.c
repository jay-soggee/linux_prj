#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#define DEBUG

typedef struct timespec Pitime;
Pitime gettime_now;
Pitime NOW() {
    clock_gettime(CLOCK_REALTIME, &gettime_now);
    return gettime_now;
}
int isTimePassed_us(Pitime ref, int time_us) {
    // 1s = 1,000,000us, 1us = 1,000ns
    int time_sec  = time_us / 1000000;
    int time_nsec = (time_us % 1000000) * 1000;
    int passed_sec = ref.tv_sec + time_sec;
    int passed_nsec = ref.tv_nsec + time_nsec;
    clock_gettime(CLOCK_REALTIME, &gettime_now);
    return (gettime_now.tv_sec < passed_sec) ? 0 : (gettime_now.tv_nsec < passed_nsec) ? 0 : 1;
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
#define ERR_OPN_SVMT    (1 << 0)
#define ERR_OPN_BZZR    (1 << 1)
#define ERR_OPN_GPIO    (1 << 2)
#define ERR_OPN_FND     (1 << 3)

int openAllDev() {
    int* cdevs[4] = {
        &dev_svmt,
        &dev_bzzr,
        &dev_gpio,
        &dev_fnd
    };
    int err = 0;

    // note : access order - [i] is faster than *
    for (int i = 0; i < 4; i++) {
        *cdevs[i] = open(cdev_dirs[i], O_RDWR);
        if (*cdevs[i] < 0) {
            printf("main : Opening %s is not Possible!\n", cdev_dirs[i]);
            err += (1 << i);
        }
    }

    return err;
}

void closeAllDev() {
    int* cdevs[4] = {
        &dev_svmt,
        &dev_bzzr,
        &dev_gpio,
        &dev_fnd
    };
    for (int i = 0; i < 4; i++) 
        if (*cdevs[i] > 0)
            close(*cdevs[i]);
}


int toggle_button_state = 0;

void buttonUpdate() {
    char            buff;
    static char     last_button_state = '0';
    static char     curr_button_state = '0';
    static Pitime   last_pushed = {0};

    read(dev_gpio, &buff, 1); // read pin 6

    if (buff != last_button_state) // if the button signal detected(pressed or noise),
        last_pushed = NOW();         
    else if (isTimePassed_us(last_button_state, 20)) // count the time a little
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

    int opn_err = openAllDev();
    if (opn_err & ERR_OPN_GPIO) goto CDevOpenFatal;

    // wait for the start button pressed (behave as toggle)
    do {buttonUpdate();} 
    while (!toggle_button_state);

    playBuzzer('a');

    // game started. wait a sec...
    Pitime time_ref = NOW();
    while (isTimePassed_us(time_ref, 2000000));

    playBuzzer('b');

    time_ref = NOW();
    while (isTimePassed_us(time_ref, 2000000));

    
CDevOpenFatal:
    closeAllDev();
    return 0;

}