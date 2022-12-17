#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h> // need -lrt option. means to use the real-time library

#define DEBUG_R
#define DEBUG

#define DEATH_MATCH  1  // do until someone is defeated
#define SERVIVAL     0  // do just three rounds
#define USER         0  // score index
#define RASPI        1
#define OFFALL    2
#define WIN       1
#define LOSE      0


typedef struct timespec Pitime;
Pitime gettime_now;
Pitime NOW() {
    clock_gettime(CLOCK_REALTIME, &gettime_now);
    return gettime_now;
}
int isTimePassed_us(Pitime* ref, int time_us) {
    // 1s = 1,000,000us, 1us = 1,000ns
    int time_sec  = time_us / 1000000;
    int time_nsec = (time_us - time_sec) * 1000;
    int passed_sec = ref->tv_sec + time_sec;
    int passed_nsec = ref->tv_nsec + time_nsec;
    if (passed_nsec > 1000000000) { // control overflow
        passed_sec++;
        passed_nsec -= 1000000000;
    }
    clock_gettime(CLOCK_REALTIME, &gettime_now);
    return (gettime_now.tv_sec < passed_sec) ? 0 : (gettime_now.tv_nsec < passed_nsec) ? 0 : 1;
}

int myRand() {
    Pitime rand_seed;
    rand_seed = NOW();
#ifdef DEBUG_R
    printf("main : rand val : %d", (rand_seed.tv_nsec & 1));
#endif
    return rand_seed.tv_nsec & 1;
}


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

int openAllDev();
void closeAllDev();


int toggle_button_state = 0;

// update toggle button signal w/ signal debouncing
void buttonUpdate() {
    const  int      debounce_time_us = 40;
           char     buff;
    static char     last_button_state = '0';
    static char     curr_button_state = '0';
    static Pitime   last_pushed = {0};

    read(dev_gpio, &buff, 1); // read pin 6

    if (buff != last_button_state) // if the button signal detected(pressed or noise),
        last_pushed = NOW();         
    else if (!isTimePassed_us(&last_pushed, debounce_time_us)) // count the time a little
        if (buff != curr_button_state) { // if the button signal is still changed
            curr_button_state = buff;
            if (curr_button_state == '1')
                toggle_button_state = !toggle_button_state;
        }
    last_button_state = buff; // last_button_state will follow the signal(pressed or noise).
}

void writeLED(const int winflag) {
    char buff = '0';
    if (winflag) {
        buff++;
        write(dev_gpio, &buff, 1);  // blue LED
    }
    else 
        write(dev_gpio, &buff, 1);  // red LED
}


void playBuzzer(char song) {
    static char prev_song = 'i';
    if (prev_song != song) {
        write(dev_bzzr, &song, 1);
        prev_song = song;
    }
}


#define D1 0x01
#define D2 0x02
#define D3 0x04
#define D4 0x08

const char seg_num[10] = {0xc0, 0xf9, 0xa4, 0xb0, 0x99, 0x92, 0x82, 0xd8, 0x80, 0x90};
const char seg_dot = 0x7f;
int FND(int* score) {
    unsigned short data[3];
    static int n = 0;

    data[0] = (seg_num[score[0]] << 4) | D1;
    data[2] = (seg_dot           << 4) | D3;
    data[1] = (seg_num[score[1]] << 4) | D4;

    write(dev_fnd, &data[n], 2);
    n++;
    n = (n + 1) % 2;
}




int main(void) {

    // open character devices
    int opn_err = openAllDev();
    if (opn_err & ERR_OPN_GPIO) goto CDevOpenFatal;
    

    // wait for the start button pressed (behave as toggle)
    do {buttonUpdate();} 
    while (!toggle_button_state);

    // game started. wait 2sec...
    Pitime time_ref = NOW();
    int game_mode = SERVIVAL;
    while (!isTimePassed_us(&time_ref, 2000000)) buttonUpdate();
    if (toggle_button_state == 0) {
        toggle_button_state = 1;
        game_mode = DEATH_MATCH;
    }

    printf("game starts, mode = %s\n", game_mode == SERVIVAL ? "SERVIVAL" : "DEATH MATCH");
    // initialize variables for loop
    int score[2] = { 3, 3 };
    int stage_count = (game_mode == SERVIVAL) ? 3 : 999999999;
    int stage_result = 1;
    int rpi_dir, usr_dir, usr_dir0, usr_dir1;
    time_ref = NOW();

    while (toggle_button_state) {
        // Face Detecting...

        FND(score);
        buttonUpdate();

        if(!isTimePassed_us(&time_ref, 700000)){ // ~0.7s

            printf("stage 1\n");
            playBuzzer('a'); //cham cham cham! (only once)

        } else if (!isTimePassed_us(&time_ref, 1400000)) { // ~1.4s

            printf("stage 2\n");
            rpi_dir = myRand(); //is current system clock count odd? or even?

        } else if (!isTimePassed_us(&time_ref, 3100000)) { // ~3.1s

            printf("stage 3\n");
            if (stage_result == 1) playBuzzer('b');  // win (user side)
            else playBuzzer('c'); //stage_result == 0   lose

        } else { // after 3.1s
            
            printf("stage 4\n");
            if (stage_result == 1) {  // win (user side)
                score[RASPI]--;
                writeLED(WIN);
            }
            else {                    // lose
                score[USER]--;
                writeLED(LOSE);
            }

            if (--stage_count && (score[RASPI] && score[USER])) {
                time_ref = NOW();
            } else { // if game ends.
                if (score[USER] >= score[RASPI])
                    playBuzzer('d'); // user won  this game
                else
                    playBuzzer('e'); // user lost this game

                break; // finish game.
            }
        }
    }
    time_ref = NOW();
    while (!isTimePassed_us(&time_ref, 2000000));

CDevOpenFatal:
    closeAllDev();

    return 0;
}








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