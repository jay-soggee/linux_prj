#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h> // need -lrt option. means to use the real-time library

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

int openAllDev();
void closeAllDev();


int toggle_button_state = 0;

// update toggle button signal w/ signal debouncing
void buttonUpdate() {
    char            buff;
    static char     last_button_state = '0';
    static char     curr_button_state = '0';
    static Pitime   last_pushed = {0};

    read(dev_gpio, &buff, 1); // read pin 6

    if (buff != last_button_state) // if the button signal detected(pressed or noise),
        last_pushed = NOW();         
    else if (!isTimePassed_us(last_pushed, 20)) // count the time a little
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
    while (!isTimePassed_us(time_ref, 2000000));
    
    // initialize variables
    int score[2] = { 0, 0 };
    Motor(0);
    time_ref = NOW();

    while (toggle_button_state){
        // Face Detecting...

        FND(score);

        /*버튼상태 업데이트*/
        buttonUpdate();

        if(/*기준부터 0.7초 전까지는...*/){
            /* 참참참 부저 울리기*/
            chamchamcham(); //얘는 한번만 실행함
                        
            Motor(0); //얘는 한번만 할 필욘 없는데 계속할 필요는 더더욱 없음

            user_dir0 = /*이용자 얼굴각도 읽어오기*/;


            //while문 안에서는 되도록 printf문 금지
            //printf("Compter : %d \t User : %d \t Result : %d", com_dir, user_dir, result);         
            //else printf("Can't Detect face...\n");

        } else if (/*그 이후, 기준부터 1.4초 전까지는...*/) {

            com_dir = /*컴퓨터 좌우 방향 선택*/;
            Motor(com_dir);

            user_dir1 = /*이용자 얼굴각도 읽어오기*/;

        } else if (/*그 이후, 기준부터 3.1초 전까지는...*/) {
            
            user_dir = user_dir0 - user_dir1;
            result_stage = Compare(com_dir, user_dir);
                
            // 마찬가지로 한번만
            stageBeep(result_stage); //스테이지 결과 재생(승/패)

        } else { //한 스테이지가 끝나고... 다음스테이지? 아님 끝?
                
            //이 else문은 스테이지가 끝나고 한번만 실행됨

            if (--stage_count) {//게임이 안끝났으면
                time_ref = NOW(); //기준시간 다시 업데이트
                /*스테이지 결과를 스코어에 반영하기*/ 

            } else {   //게임이 끝났으면

                switch (result) {
                case WIN:
                    /* 파란색 LED*/ // usleep(delay_time);?
                    /* 승리 부저*/
                    score[0]++;
                    break;
                case LOSE:
                    /* 빨간색 LED*/
                    /* 패배 부저*/
                    score[1]++;
                    break;
                case DRAW:
                    /* */
                    printf("Draw Game\n");
                default:
                    break;
                }

            }
        }
        
        /* 초기화 상태로 만들기 -재석: 근데 이렇게하면 모터가 항상 0도가 됨*/ 
        //Motor(0);
        //LED(0,0);
    }

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