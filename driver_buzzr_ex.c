#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/pwm.h>
#include <linux/gpio.h>


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Johannes 4 GNU/Linux");
MODULE_DESCRIPTION("A simple driver to access the Hardware PWM IP");

/* Variables for device and device class */
static dev_t my_device_nr;
static struct class *my_class;
static struct cdev my_device;

#define DRIVER_NAME "my_pwm_driver"
#define DRIVER_CLASS "MyModuleClass"

/* Variables for pwm  */
struct pwm_device *pwm0 = NULL;
u32 pwm_on_time = 500000000;

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
	if(value < 'a' || value > 'j')
		printk("Invalid Value\n");
	else
		pwm_config(pwm0, 100000000 * (value - 'a'), 1000000000);

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
	printk("Device class created!\n");
	
	/* create device file */
	if(device_create(my_class, NULL, my_device_nr, NULL, DRIVER_NAME) == NULL) {
		printk("Can not create device file!\n");
		goto FileError;
	}
	printk("Device file created!\n");

	/* Initialize device file */
	cdev_init(&my_device, &fops);

	/* Regisering device to kernel */
	if(cdev_add(&my_device, my_device_nr, 1) == -1) {
		printk("Registering of device to kernel failed!\n");
		goto AddError;
	}
	printk("Registering of device to kernel success!\n");

///////////////////// port request /////////////////////

	pwm0 = pwm_request(0, "my-pwm");
	if(pwm0 == NULL) {
		printk("Could not get PWM0!\n");
		goto AddError;
	}
	printk("Requesting PWM0 success!\n");

	/* GPIO 12 init */
	if(gpio_request(12, "rpi-gpio-12")) {
		printk("Can not allocate GPIO 12\n");
		goto PwmError;
	}
	printk("Requesting GPIO 12 success!\n");

	/* Set GPIO 12 direction */
	if(gpio_direction_output(12, 0)) {
		printk("Can not set GPIO 12 to output!\n");
		goto Gpio12Error;
	}

	/* GPIO 13 init */
	if(gpio_request(13, "rpi-gpio-13")) {
		printk("Can not allocate GPIO 13\n");
		goto Gpio12Error;
	}
	printk("Requesting GPIO 13 success!\n");

	/* Set GPIO 13 direction */
	if(gpio_direction_output(13, 0)) {
		printk("Can not set GPIO 13 to output!\n");
		goto Gpio13Error;
	}

	printk("All GPIO set, configuring pwm0\n");
	pwm_config(pwm0, pwm_on_time, 1000000000);
	printk("Enabling pwm0\n");
	pwm_enable(pwm0);
	printk("pmw0 set, setting GPIO\n");
	gpio_set_value(12, 1);
	gpio_set_value(13, 1);
	printk("Module_init successfully returns");
	
	return 0;

Gpio13Error:
	gpio_free(13);
Gpio12Error:
	gpio_free(12);
PwmError:
	pwm_disable(pwm0);
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
	gpio_set_value(13, 0);
	gpio_set_value(12, 0);
	gpio_free(13);
	gpio_free(12);
	pwm_disable(pwm0);
	pwm_free(pwm0);
	cdev_del(&my_device);
	device_destroy(my_class, my_device_nr);
	class_destroy(my_class);
	unregister_chrdev_region(my_device_nr, 1);
	printk("Goodbye, Kernel\n");
}

module_init(ModuleInit);
module_exit(ModuleExit);
