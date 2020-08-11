#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/kprobes.h>
#include <linux/blkdev.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <linux/gpio.h>
#include <asm/uaccess.h>

#include "gpio-control.h"

#define DRIVER_NAME "gpio_control"
static unsigned int gpio_control_major = 0;
static unsigned int num_of_dev = 1;
static struct cdev gpio_control_cdev;
static int ioctl_num = 0;
static dev_t dev; 

/* gpio configuration */
static unsigned int gpio_number = 23; /* Change it to the gpio port that you wanna control*/

/* construct kobject */
static struct kobject *kobject;
int gpio_state = 0;

static ssize_t gpio_state_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", gpio_state);
}

static ssize_t gpio_state_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t len)
{
	sscanf(buf, "%d", &gpio_state);
	return len;
}

static struct kobj_attribute gpio_state_attribute = __ATTR_RW(gpio_state);

/* ioctl functions */
static int gpio_control_open(struct inode *inode, struct file *filp)
{
	printk(KERN_INFO "%s open \n", DRIVER_NAME);
	return 0;
}

static int gpio_control_close(struct inode *inode, struct file *filp)
{
	printk(KERN_ALERT "%s call.\n", __func__);
	printk(KERN_INFO "%s release \n", DRIVER_NAME);	
	return 0;
}

static long gpio_control_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	int pinValue;
	switch(cmd) {
		case IOCTL_UP:
			printk(KERN_INFO "Detect UP operation. write %d to %d\n", GPIOF_INIT_HIGH, gpio_number);	
			SYNO_GPIO_WRITE(gpio_number, GPIOF_INIT_HIGH);
			break;
		case IOCTL_DOWN:
			printk(KERN_INFO "Detect DOWN operation. write %d to %d\n", GPIOF_INIT_LOW, gpio_number);	
			SYNO_GPIO_WRITE(gpio_number, GPIOF_INIT_LOW);
			break;
		case IOCTL_GET:
			pinValue = SYNO_GPIO_READ(gpio_number);
			printk(KERN_INFO "Detect GET operation. read %d from %d\n", pinValue, gpio_number);	
			ret = __put_user(pinValue, (int __user *)arg);
			break;
		default:
			printk(KERN_WARNING "%s driver(major: %d) No such operation.\n", DRIVER_NAME, gpio_control_major);
			break;
	}
	return ret;
}

struct file_operations fops = {
 .owner = THIS_MODULE,
 .open = gpio_control_open,
 .release = gpio_control_close,
 .unlocked_ioctl = gpio_control_ioctl,
};

static int __init gpio_control_init(void)
{
	/* register kobject */
	int error;
	kobject = kobject_create_and_add("gpio-control", kernel_kobj);
    if(!kobject)
		return -ENOMEM;

	error = sysfs_create_file(kobject, &gpio_state_attribute.attr);
	if (error) {
		printk(KERN_WARNING "sysfs_create_file failed\n");
	}
	printk(KERN_WARNING "Sysfs /sys/kernel/gpio-control/gpio_state created\n");

	/* register gpio */
	gpio_request(gpio_number, "sysfs");
	gpio_direction_output(gpio_number, GPIOF_INIT_HIGH);
	gpio_export(gpio_number, false);

	/* register cdev */
	dev = MKDEV(gpio_control_major, 0);
	int alloc_ret = 0;
	int cdev_ret = 0;

	alloc_ret = alloc_chrdev_region(&dev, 0, num_of_dev, DRIVER_NAME);
	if(alloc_ret < 0) {
		if (cdev_ret == 0) cdev_del(&gpio_control_cdev);
		printk(KERN_ALERT "%s driver: alloc_chrdev_region error.\n", DRIVER_NAME);
	}

	gpio_control_major = MAJOR(dev);

	cdev_init(&gpio_control_cdev, &fops);
	cdev_ret = cdev_add(&gpio_control_cdev, dev, num_of_dev);
	if(cdev_ret < 0) {
		if (alloc_ret == 0) unregister_chrdev_region(dev, num_of_dev);
		printk(KERN_ALERT "%s driver: cdev_add error.\n", DRIVER_NAME);
	}

	printk(KERN_WARNING "%s driver(major: %d) installed.\n", DRIVER_NAME, gpio_control_major);
	return 0;
}

static void __exit gpio_control_exit(void)
{
	unregister_chrdev_region(dev, num_of_dev);
	cdev_del(&gpio_control_cdev);
	sysfs_remove_file(kobject, &gpio_state_attribute.attr);
	kobject_put(kobject);
	kobject_del(kobject);
	printk(KERN_WARNING "%s driver(major: %d) uninstalled.\n", DRIVER_NAME, gpio_control_major);
}

module_init(gpio_control_init)
module_exit(gpio_control_exit)
MODULE_LICENSE("GPL");
