#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h> // need -lrt option. means to use the real-time library

/****** AI ******/
#include <vector>
#include <iostream>
#include <fstream>

#include <opencv2/opencv.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/objdetect/objdetect.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/photo.hpp>

#include "ldmarkmodel.h"

using namespace std;
using namespace cv;

#define head_pose_eav0      0
#define head_pose_eav1      1
#define head_pose_eav_off0  2
#define head_pose_eav_off1  3

#define HEAD_CENTER         2
#define HEAD_LEFT           0
#define HEAD_RIGHT          1

/****************/

//#define DEBUG_R
//#define DEBUG
//#define DEBUG_BTN
//#define DEBUG_TIME

#define DEATH_MATCH     1  // do until someone is defeated
#define SERVIVAL        0  // do just three rounds
#define USER        0  // score index
#define RASPI       1

////////////////////// clock timer //////////////////////

#define SEC2nSEC    1000000000
#define SEC2uSEC    1000000

typedef struct timespec Pitime;
Pitime gettime_now;
Pitime NOW() {
    clock_gettime(CLOCK_REALTIME, &gettime_now);
    return gettime_now;
}

int timePassed_us(Pitime* ref) {
    int time_passed_nsec;
    int time_passed_sec;
    int underflow;
    
    // 1s = 1,000,000us, 1us = 1,000ns
    clock_gettime(CLOCK_REALTIME, &gettime_now);
    time_passed_nsec  = gettime_now.tv_nsec - ref->tv_nsec;
    underflow = time_passed_nsec < 0 ? 1 : 0;
    if (underflow) time_passed_nsec += 1 * SEC2nSEC;
    time_passed_sec   = gettime_now.tv_sec  - ref->tv_sec - underflow;
#ifdef DEBUG_TIME
    if (time_passed_sec < 0) printf("timePassed_us : !\n");
#endif
    return time_passed_sec * SEC2uSEC + time_passed_nsec / 1000;
}

int myRand() {
    Pitime rand_seed;
    rand_seed = NOW();
#ifdef DEBUG_R
    printf("main : rand val : %d\n", (rand_seed.tv_nsec & 1));
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


////////////////////// GPIO //////////////////////

#define LED_OFF         2
#define LED_WIN         1
#define LED_LOSE        0

int toggle_button_state = 0;

// update toggle button signal w/ signal debouncing
void updateButton() {
    const  int      debounce_time_us = 80;
           char     buff;
    static char     last_button_state = '0';
    static char     curr_button_state = '0';
    static Pitime   last_pushed = {0};

    read(dev_gpio, &buff, 1); // read pin 6

    if (buff != last_button_state) // if the button signal detected(pressed or noise),
        last_pushed = NOW();         
    if (timePassed_us(&last_pushed) > debounce_time_us){ // count the time a little
        if (buff != curr_button_state) { // if the button signal is still changed
            curr_button_state = buff;
            if (curr_button_state == '1') {
#ifdef DEBUG_TIME
                printf("updateButton : pressed\n");
#endif
                toggle_button_state = !toggle_button_state;
            }
        }
    }
    last_button_state = buff; // last_button_state will follow the signal(pressed or noise).
}

void writeLED(const int winflag) {
    static int prev_winflag = 0;
    if (prev_winflag == winflag) return;

    char buff;
    if      (winflag == LED_WIN)    buff = '1'; // blue LED
    else if (winflag == LED_OFF)    buff = '2'; // turn all LED off
    else  /*(winflag == LED_LOSE)*/ buff = '0'; // red LED

    write(dev_gpio, &buff, 1);

    prev_winflag = winflag;
}


////////////////////// Buzzer //////////////////////

void playBuzzer(char song) {
    static char prev_song = 'i';
    if (prev_song == song) return;

    write(dev_bzzr, &song, 1);

    prev_song = song;
}


////////////////////// 7-Segment //////////////////////

#define D1 0x01
#define D2 0x02
#define D3 0x04
#define D4 0x08
char seg_num[10] = {0xc0, 0xf9, 0xa4, 0xb0, 0x99, 0x92, 0x82, 0xd8, 0x80, 0x90};
const char seg_dot = 0x7f;

void FND(int* score) { //TODO: FND in trouble
    unsigned short data[3];
    static int n = 0;

    data[0] = (seg_num[score[USER ]] << 4) | D1;
    data[1] = (seg_num[score[RASPI]] << 4) | D4;
    data[2] = (seg_dot               << 4) | D2;

    write(dev_fnd, &data[n], 2);
    n = (n + 1) % 3;
}


////////////////////// Servo Motor //////////////////////

#define MTR_CENT        2
#define MTR_LEFT        0
#define MTR_RIGT        1

void setMotor(int motor_dir){
    static int prev_dir = 3;
    if (prev_dir == motor_dir) return;

    char buff = '0';
    if      (motor_dir == MTR_LEFT)     buff = '0'; // Turn Left
    else if (motor_dir == MTR_RIGT)     buff = '2'; // Turn Right
    else  /*(motor_dir == MTR_CENT)*/   buff = '1'; // Center

    write(dev_svmt, &buff, 1);

    prev_dir = motor_dir;
}


////////////////////// main //////////////////////

int main(void) {
    Pitime time_ref;

    // AI Dataset Open && Camera open
    ldmarkmodel modelt;
    std::string modelFilePath = "roboman-landmark-model.bin";

    while (!load_ldmarkmodel(modelFilePath, modelt)) {
        std::cout << "load_ldmarkmodel error." << std::endl;
        std::cin >> modelFilePath;
    }

    cv::VideoCapture mCamera;

    mCamera.open("/dev/video0", CAP_V4L2); // ***
    if (!mCamera.isOpened()) {
        printf("Can`t open Camera\n");
        return -1;
    }

    // For Face Detection && Face Landmark Point
    cv::Mat Image;
    cv::Mat current_shape;

    int eav_status;
    int head_pose_0, head_pose_1;
    int head_dir;
    
    // open character devices
    int opn_err = openAllDev();
    

    // wait for the start button pressed (behave as toggle)
    do { updateButton(); } 
    while (!toggle_button_state);

    // game started. wait 2sec...
    int game_mode = SERVIVAL;
    time_ref = NOW();
    while (timePassed_us(&time_ref) < (2 * SEC2uSEC)) updateButton();
    if (toggle_button_state == 0) {
        toggle_button_state = 1;
        game_mode = DEATH_MATCH;
    }
#ifdef DEBUG
    printf("main : game starts. curr game mode : %s\n", game_mode == SERVIVAL ? "SERVIVAL" : "DEATH_MATCH");
#endif

    // initialize variables for loop
    int passed_time_from_ref;
    int score[2] = { 3, 3 };
    int stage_count = (game_mode == SERVIVAL) ? 3 : 999999999;
    int stage_result = 1;
    int rpi_dir, usr_dir, usr_dir0, usr_dir1;
    time_ref = NOW();
    setMotor(MTR_CENT);
    eav_status = head_pose_eav0;
#ifdef DEBUG
    int current = 0;
#endif

    while (toggle_button_state) {

        FND(score);
        updateButton();
        passed_time_from_ref = timePassed_us(&time_ref);

        // Face Detection && Landmark Point Traking
        mCamera.read(Image);
        if (Image.empty()) break;

        modelt.track(Image, current_shape);
        cv::Vec3d eav;
        

        //************* switch(passed_time) *************//
        if (passed_time_from_ref < (1.4 * SEC2uSEC)){ 


#ifdef DEBUG
            if (current != 1) {printf("Stage 1\n"); current = 1;}
#endif
            playBuzzer('a'); //cham cham cham! (only once)
            rpi_dir = myRand(); //is current system clock count odd? or even?
            setMotor(MTR_CENT);
            //FIXME: get user head pose 0.


            //////////////    ~0.7s    //////////////
        } else if (passed_time_from_ref < (2.8 * SEC2uSEC)) { 
            if(eav_status == head_pose_eav0) {
                modelt.EstimateHeadPose(current_shape, eav);
                head_pose_0 = eav[1];
                eav_status = head_pose_eav1;
#ifdef DEBUG
                printf("head_pose_0 : %d\n", head_pose_0);
#endif
            }


#ifdef DEBUG
            if (current != 2) {printf("Stage 2 : rpi_dir = %d, usr_dir0 = \n", rpi_dir); current = 2;}
#endif
            if (passed_time_from_ref < (2.3 * SEC2uSEC)) 
                setMotor(rpi_dir);
            //FIXME: get user head pose 1.


            //////////////    ~1.4s    //////////////
        } else if (passed_time_from_ref < (7 * SEC2uSEC)) {
            if(eav_status == head_pose_eav1) {
                modelt.EstimateHeadPose(current_shape, eav);
                head_pose_1 = eav[1];
                eav_status = head_pose_eav_off0; 
#ifdef DEBUG
                printf("head_pose_1 : %d\n", head_pose_1); 
#endif              
            }
            

#ifdef DEBUG
            if (current != 3) {printf("Stage 3 : usr_dir1 = \n"); current = 3;}
#endif
            if(eav_status == head_pose_eav_off1) {
                //FIXME: is_result_computed
                if (stage_result == 1) {  // win (user side) 
                    playBuzzer('b');
                    writeLED(LED_WIN);
                }
                else { //stage_result == 0   lose
                    playBuzzer('c');
                    writeLED(LED_LOSE);
                }
            }
            //FIXME: compute user's decision and update the result.
            else { // if(eav_status == head_pose_eav_off0)  
                if((head_pose_0 - head_pose_1) < 0) head_dir = HEAD_RIGHT;
                else head_dir = HEAD_LEFT;
#ifdef DEBUG
                printf("head_dir    : %d\n", head_dir);
#endif
                if(rpi_dir == head_dir) stage_result = 0; // rip win
                else stage_result = 1; // usr win

                eav_status = head_pose_eav_off1;
            }

            //////////////    ~4.1s    //////////////
        } else {
            ////////////// after 4.1s  //////////////

            eav_status = head_pose_eav0;
#ifdef DEBUG
            if (current != 4) {printf("Stage 4\n"); current = 4;}
#endif  
            writeLED(LED_OFF);
            if (stage_result == 1) score[RASPI]--; // win (user side)
            else                   score[USER ]--; // lose

            if ((--stage_count >= 0) && (score[RASPI] && score[USER])) {
                // if the game isn't over
                time_ref = NOW();
            } else { // if the game is over.
                if (score[USER] >= score[RASPI])
                    playBuzzer('d'); // user won  this game
                else
                    playBuzzer('e'); // user lost this game

                break; // finish game.
            }
        }
    }
    time_ref = NOW();
    while (timePassed_us(&time_ref) < (3 * SEC2uSEC));
    writeLED(LED_OFF);
    setMotor(MTR_CENT);


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