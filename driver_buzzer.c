#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/pwm.h>
#include <linux/jiffies.h>
#include <linux/timer.h>

//#define DEBUG

/* Meta Information */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Johannes 4 GNU/Linux");
MODULE_DESCRIPTION("A simple driver to access the Hardware PWM IP");

/* Variables for device and device class */
static dev_t my_device_nr;
static struct class *my_class;
static struct cdev my_device;
static struct timer_list my_timer;

#define DRIVER_NAME "my_buzzer_driver"
#define DRIVER_CLASS "MyModuleClass"

/* Variables */
struct pwm_device *pwm1 = NULL;

///// MIDI Configurations /////
#define BPM108 70
#define BPM126 60
#define NOTE_BREAK 1000000000
unsigned long note_durations[5][7] = {
    { 3, 5, 3, 5, 1, },
    { 1, 1, 1, 1, 1, },
    { 1, 1, 6, },
    { 2, 2, 2, 2, 4, },
    { 2, 2, 2, 2, 2, 2, }
};
unsigned long note_scales[5][7] = {
    { 6811989, NOTE_BREAK, 6811989, NOTE_BREAK, 4545454, NOTE_BREAK },  // cham, cham, cham!
    { 6428243, NOTE_BREAK, 5102101, NOTE_BREAK, 4290337, NOTE_BREAK },  // win!
    { 6428243, NOTE_BREAK, 6428243, NOTE_BREAK },                       // lose...
    { 6428243, 4815742, 3822255, 6428243, 4815742, NOTE_BREAK },        // victory!
    { 3822255, 4545454, 5726914, 3822255, 4545454, 5726914, NOTE_BREAK }// mission failed...
};
unsigned long* now_playing_duration = NULL, * now_playing_scale = NULL;
void genNotes(void) {
    int i, j;
    // BPM108
    for (i = 0; i < 3; i++) 
        for (j = 0; j < 6; j++)
            note_durations[i][j] = note_durations[i][j] * msecs_to_jiffies(BPM108);
    // BPM126
    for (i = 3; i < 5; i++) 
        for (j = 0; j < 6; j++)
            note_durations[i][j] = note_durations[i][j] * msecs_to_jiffies(BPM126);
#ifdef DEBUG
    for (i = 0; i < 5; i++) 
        for (j = 0; j < 6; j++)
            printk("%lu\n",note_durations[i][j]);
#endif
}

/// @brief interrupt callback function.
void play_next_node(struct timer_list * data) {
    unsigned long play_dur = *(++now_playing_duration);
    unsigned long play_scl = *(++now_playing_scale);
    if (!play_dur) {     // if playing this song is done.
        pwm_disable(pwm1);
    }
    else {               // if not, play it.
        if (play_scl == NOTE_BREAK)
            pwm_disable(pwm1);
        else {
            pwm_config(pwm1, play_scl / 2, play_scl);
            pwm_enable(pwm1);
        }
        mod_timer(&my_timer, jiffies + play_dur);
    }
}


/**
 * @brief Write data to buffer
 */
static ssize_t driver_write(struct file *File, const char *user_buffer, size_t count, loff_t *offs) {
    int to_copy, not_copied, delta;
    char value;

    /* Get amount of data to copy */
    to_copy = min(count, sizeof(value));

    /* Copy data to user */
    not_copied = copy_from_user(&value, user_buffer, to_copy);

    /* Set PWM on time */
    switch (value) {
        case 'a':
            printk("buzzer driver : song a\n");
            now_playing_duration = note_durations[0];
            now_playing_scale = note_scales[0];
            break;
        case 'b':
            printk("buzzer driver : song b\n");
            now_playing_duration = note_durations[1];
            now_playing_scale = note_scales[1];
            break;
        case 'c':
            printk("buzzer driver : song c\n");
            now_playing_duration = note_durations[2];
            now_playing_scale = note_scales[2];
            break;
        case 'd':
            printk("buzzer driver : song d\n");
            now_playing_duration = note_durations[3];
            now_playing_scale = note_scales[3];
            break;
        case 'e':
            printk("buzzer driver : song e\n");
            now_playing_duration = note_durations[4];
            now_playing_scale = note_scales[4];
            break;
        default:
            printk("buzzer driver : Invalid Value\n");
            break;
    }

    pwm_config(pwm1, *(now_playing_scale) / 2, *(now_playing_scale));
    pwm_enable(pwm1);
    mod_timer(&my_timer, jiffies + *(now_playing_duration));

    /* Calculate data */
    delta = to_copy - not_copied;

    return delta;
}

/**
 * @brief This function is called, when the device file is opened
 */
static int driver_open(struct inode *device_file, struct file *instance) {
    printk("dev_nr - open was called!\n");
    return 0;
}

/**
 * @brief This function is called, when the device file is opened
 */
static int driver_close(struct inode *device_file, struct file *instance) {
    printk("dev_nr - close was called!\n");
    return 0;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = driver_open,
    .release = driver_close,
    .write = driver_write
};

/**
 * @brief This function is called, when the module is loaded into the kernel
 */
static int __init ModuleInit(void) {
    printk("Hello, Kernel!\n");

    /* Allocate a device nr */
    if( alloc_chrdev_region(&my_device_nr, 0, 1, DRIVER_NAME) < 0) {
        printk("Device Nr. could not be allocated!\n");
        return -1;
    }
    printk("read_write - Device Nr. Major: %d, Minor: %d was registered!\n", my_device_nr >> 20, my_device_nr && 0xfffff);

    /* Create device class */
    if((my_class = class_create(THIS_MODULE, DRIVER_CLASS)) == NULL) {
        printk("Device class can not be created!\n");
        goto ClassError;
    }

    /* create device file */
    if(device_create(my_class, NULL, my_device_nr, NULL, DRIVER_NAME) == NULL) {
        printk("Can not create device file!\n");
        goto FileError;
    }

    /* Initialize device file */
    cdev_init(&my_device, &fops);

    /* Regisering device to kernel */
    if(cdev_add(&my_device, my_device_nr, 1) == -1) {
        printk("Registering of device to kernel failed!\n");
        goto AddError;
    }

    ////////////// initialize pwm 1 //////////////
    pwm1 = pwm_request(0, "my-pwm");
    if(pwm1 == NULL) {
        printk("Could not get PWM1!\n");
        goto AddError;
    }

    // /* GPIO 13 init */
	// if(gpio_request(13, "rpi-gpio-13")) {
	// 	printk("Can not allocate GPIO 13\n");
	// 	goto AddError;
	// }

	// /* Set GPIO 13 direction */
	// if(gpio_direction_output(13, 0)) {
	// 	printk("Can not set GPIO 13 to output!\n");
	// 	goto Gpio13Error;
	// }

    // gpio_set_value(13, 1);

    genNotes();
	timer_setup(&my_timer, play_next_node, 0);

    return 0;
// Gpio4Error:
//     gpio_free(13);
AddError:
    device_destroy(my_class, my_device_nr);
FileError:
    class_destroy(my_class);
ClassError:
    unregister_chrdev_region(my_device_nr, 1);
    return -1;
}

/**
 * @brief This function is called, when the module is removed from the kernel
 */
static void __exit ModuleExit(void) {
    // gpio_set_value(13, 0);
    // gpio_free(13);
    pwm_disable(pwm1);
    pwm_free(pwm1);
    cdev_del(&my_device);
    device_destroy(my_class, my_device_nr);
    class_destroy(my_class);
    unregister_chrdev_region(my_device_nr, 1);
    printk("Goodbye, Kernel\n");
}

module_init(ModuleInit);
module_exit(ModuleExit);