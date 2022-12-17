#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>

/* Meta Information */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Emb. Sys. prj 4 GNU/Linux");
MODULE_DESCRIPTION("A simple gpio driver for setting a LED and reading a button");

/* Variables for device and device class */
static dev_t my_device_nr;
static struct class *my_class;
static struct cdev my_device;

#define DRIVER_NAME "my_gpio_driver"
#define DRIVER_CLASS "MyModuleClass"

/**
 * @brief Read data out of the buffer
 */
static ssize_t driver_read(struct file *File, char *user_buffer, size_t count, loff_t *offs) {
	int to_copy, not_copied, delta;
	char tmp = 0;

	/* Get amount of data to copy */
	to_copy = min(count, sizeof(tmp));

	/* Read value of button */
	tmp = gpio_get_value(6) + '0';

	/* Copy data to user */
	not_copied = copy_to_user(user_buffer, &tmp, to_copy);

	/* Calculate data */
	delta = to_copy - not_copied;

	return delta;
}

static ssize_t driver_write(struct file *File, const char *user_buffer, size_t count, loff_t *offs) {
    int to_copy, not_copied, delta;
    unsigned char result = 0;

    /*Get amount of data to copy*/
    to_copy = min(count, sizeof(result));

    /*Copy data to user*/
    not_copied = copy_from_user(&result, user_buffer, to_copy);

    switch (result) {
    case '1':
        gpio_set_value(10, 1);
        break;
    case '0':
        gpio_set_value(9, 1);
		break;
    case '2':
        gpio_set_value(10, 0);
        gpio_set_value(9, 0);
		break;
    default:
		printk("Invalid Input!\n");
        break;
    }
    /* Calculate data */
	delta = to_copy - not_copied;

	return delta;
}
/**
 * @brief This function is called, when the device file is opened
 */
static int driver_open(struct inode *device_file, struct file *instance) {
	printk("gpio_driver - open was called!\n");
	return 0;
}

/**
 * @brief This function is called, when the device file is opened
 */
static int driver_close(struct inode *device_file, struct file *instance) {
	printk("gpio_driver - close was called!\n");
	return 0;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = driver_open,
	.release = driver_close,
	.read = driver_read,
	.write = driver_write
};

/**
 * @brief This function is called, when the module is loaded into the kernel
 */
static int __init ModuleInit(void) {
	printk("Hello, gpio Kernel!\n");

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

	/* GPIO pinBOTTON init */
	if(gpio_request(6, "rpi-gpio-6")) {
		printk("Can not allocate GPIO pinBOTTON\n");
		goto AddError;
	}

	/* Set GPIO pinBOTTON direction */
	if(gpio_direction_input(6)) {
		printk("Can not set GPIO pinBOTTON to input!\n");
		goto Gpio6Error;
	}


	/* GPIO pinRED init */
	if(gpio_request(9, "rpi-gpio-9")) {
		printk("Can not allocate GPIO pinRED\n");
		goto Gpio6Error;
	}

	/* Set GPIO pinRED direction */
	if(gpio_direction_output(9, 0)) {
		printk("Can not set GPIO pinRED to output!\n");
		goto Gpio9Error;
	}

    /* GPIO pinBLUE init */
	if(gpio_request(10, "rpi-gpio-10")) {
		printk("Can not allocate GPIO pinBLUE\n");
		goto Gpio9Error;
	}

	/* Set GPIO pinBLUE direction */
	if(gpio_direction_output(10, 0)) {
		printk("Can not set GPIO pinBLUE to output!\n");
		goto Gpio10Error;
	}

	return 0;

Gpio10Error:
	gpio_free(10);
Gpio9Error:
	gpio_free(9);
Gpio6Error:
	gpio_free(6);
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
	gpio_set_value(9, 0);
	gpio_set_value(10, 0);
    gpio_free(10);
    gpio_free(9);
	gpio_free(6);
	cdev_del(&my_device);
	device_destroy(my_class, my_device_nr);
	class_destroy(my_class);
	unregister_chrdev_region(my_device_nr, 1);
	printk("Goodbye, gpio Kernel\n");
}

module_init(ModuleInit);
module_exit(ModuleExit);
