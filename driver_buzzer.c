#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/pwm.h>
#include <linux/jiffies.h>

#define DEBUG

/* Meta Information */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Johannes 4 GNU/Linux");
MODULE_DESCRIPTION("A simple driver to access the Hardware PWM IP");

/* Variables for device and device class */
static dev_t my_device_nr;
static struct class *my_class;
static struct cdev my_device;

#define DRIVER_NAME "my_pwm_driver"
#define DRIVER_CLASS "MyModuleClass"

/* Variables */
struct pwm_device *pwm1 = NULL;
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
unsigned long note_scale[5][6] = {
    { 6811989, NOTE_BREAK, 6811989, NOTE_BREAK, 4545454, }, // cham, cham, cham!
    { 6428243, NOTE_BREAK, 5102101, NOTE_BREAK, 4290337, }, // win!
    { 6428243, NOTE_BREAK, 6428243, },                      // lose...
    { 6428243, 4815742, 3822255, 6428243, 4815742, },       // victory!
    { 3822255, 4545454, 5726914, 3822255, 4545454, 5726914 }// mission failed...
};
unsigned long now_playing_until = 0, * now_playing = NULL;
void genNotes() {
    // BPM108
    for (int i = 0; i < 3; i++) 
        for (int j = 0; j < 6; j++)
            note_durations[i][j] = note_durations[i][j] * msecs_to_jiffies(BPM108);
    // BPM126
    for (int i = 3; i < 5; i++) 
        for (int j = 0; j < 6; j++)
            note_durations[i][j] = note_durations[i][j] * msecs_to_jiffies(BPM126);
#ifdef DEBUG
    for (int i = 0; i < 5; i++) 
        for (int j = 0; j < 6; j++)
            printk("%lu\n",note_durations[i][j]);
#endif
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
            if (!now_playing_until) {
                now_playing = note_durations[0];
            } else if (now_playing_until < jiffies) {
                now_playing += 1;
            }




            pwm_config(pwm1, FRONT, PWM_PERIOD);
            break;
        case 'l':
            pwm_config(pwm0, LEFT,  PWM_PERIOD);
            break;
        case 'r':
            pwm_config(pwm0, RIGHT, PWM_PERIOD);
            break;
        default:
            printk("Invalid Value\n");
            break;
    }

    now_playing_until = jiffies + *now_playing;

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
    pwm1 = pwm_request(1, "my-pwm");
    if(pwm1 == NULL) {
        printk("Could not get PWM1!\n");
        goto AddError;
    }
    genNotes();


    return 0;
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