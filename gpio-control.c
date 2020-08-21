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
static unsigned int iGPIOControlMajor = 0;
static unsigned int icDev = 1;
static struct cdev GPIOCdev;
static dev_t dev; 

/* gpio configuration */
static unsigned int irgGPIOPins[5] = {0, 0, 21, 22, 23}; /* Change it to the gpio port that you wanna control*/

/* construct kobject */
static struct kobject *kobject;
int iGPIOState = 0;

static ssize_t iGPIOState_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", iGPIOState);
}

static ssize_t iGPIOState_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t len)
{
	sscanf(buf, "%d", &iGPIOState);
	return len;
}

static struct kobj_attribute GPIOStateAttribute = __ATTR_RW(iGPIOState);

/* ioctl functions */
static int GPIOControlOpen(struct inode *inode, struct file *filp)
{
	printk(KERN_INFO "%s open \n", DRIVER_NAME);
	return 0;
}

static int GPIOControlClose(struct inode *inode, struct file *filp)
{
	printk(KERN_ALERT "%s call.\n", __func__);
	printk(KERN_INFO "%s release \n", DRIVER_NAME);	
	return 0;
}

static ssize_t GPIOControlWrite(struct file *file, const char __user *buf, size_t count, loff_t *ppos){
	char chpGPIOBuf[10];
	char command[10];
	int slot;

	if(count > 10)
		return -EINVAL;
	unsigned long ret;
	ret = copy_from_user(chpGPIOBuf, buf, count);
	sscanf(chpGPIOBuf, "%d %s", &slot, command);
	printk(KERN_INFO "Get command: Let slot %d goes %s\n", slot, command);

	if (slot > 4 || slot < 2) {
		printk(KERN_WARNING "The slot number %d is illegal\n", slot);
		return count;
	}

	if (strcmp(command, "up") == 0) {
		SYNO_GPIO_WRITE(irgGPIOPins[slot], GPIOF_INIT_HIGH);
	} else if (strcmp(command, "down") == 0) {
		SYNO_GPIO_WRITE(irgGPIOPins[slot], GPIOF_INIT_LOW);
	
	} else {
		printk(KERN_INFO "No such command: %s\n", command);
	}
	return count;
}

static long GPIOControlIoctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	int iPinValue;
	switch(cmd) {
		case IOCTL_UP:
			printk(KERN_INFO "Detect UP operation. write %d to %d\n", GPIOF_INIT_HIGH, irgGPIOPins[4]);	
			SYNO_GPIO_WRITE(irgGPIOPins[4], GPIOF_INIT_HIGH);
			break;
		case IOCTL_DOWN:
			printk(KERN_INFO "Detect DOWN operation. write %d to %d\n", GPIOF_INIT_LOW, irgGPIOPins[4]);	
			SYNO_GPIO_WRITE(irgGPIOPins[4], GPIOF_INIT_LOW);
			break;
		case IOCTL_GET:
			iPinValue = SYNO_GPIO_READ(irgGPIOPins[4]);
			printk(KERN_INFO "Detect GET operation. read %d from %d\n", iPinValue, irgGPIOPins[4]);	
			ret = __put_user(iPinValue, (int __user *)arg);
			break;
		default:
			printk(KERN_WARNING "%s driver(major: %d) No such operation.\n", DRIVER_NAME, iGPIOControlMajor);
			break;
	}
	return ret;
}

struct file_operations fops = {
 .owner = THIS_MODULE,
 .open = GPIOControlOpen,
 .write = GPIOControlWrite,
 .release = GPIOControlClose,
 .unlocked_ioctl = GPIOControlIoctl,
};

static int __init GPIOControlInit(void)
{
	/* register kobject */
	int error;
	kobject = kobject_create_and_add("gpio-control", kernel_kobj);
        if(!kobject)
		return -ENOMEM;

	error = sysfs_create_file(kobject, &GPIOStateAttribute.attr);
	if (error) {
		printk(KERN_WARNING "sysfs_create_file failed\n");
	}
	printk(KERN_WARNING "Sysfs /sys/kernel/gpio-control/gpio_state created\n");

	/* register cdev */
	dev = MKDEV(iGPIOControlMajor, 0);
	int alloc_ret = 0;
	int cdev_ret = 0;

	alloc_ret = alloc_chrdev_region(&dev, 0, icDev, DRIVER_NAME);
	if(alloc_ret < 0) {
		if (cdev_ret == 0) cdev_del(&GPIOCdev);
		printk(KERN_ALERT "%s driver: alloc_chrdev_region error.\n", DRIVER_NAME);
	}

	iGPIOControlMajor = MAJOR(dev);

	cdev_init(&GPIOCdev, &fops);
	cdev_ret = cdev_add(&GPIOCdev, dev, icDev);
	if(cdev_ret < 0) {
		if (alloc_ret == 0) unregister_chrdev_region(dev, icDev);
		printk(KERN_ALERT "%s driver: cdev_add error.\n", DRIVER_NAME);
	}

	printk(KERN_WARNING "%s driver(major: %d) installed.\n", DRIVER_NAME, iGPIOControlMajor);
	return 0;
}

static void __exit GPIOControlExit(void)
{
	unregister_chrdev_region(dev, icDev);
	cdev_del(&GPIOCdev);
	sysfs_remove_file(kobject, &GPIOStateAttribute.attr);
	kobject_put(kobject);
	kobject_del(kobject);
	printk(KERN_WARNING "%s driver(major: %d) uninstalled.\n", DRIVER_NAME, iGPIOControlMajor);
}

module_init(GPIOControlInit)
module_exit(GPIOControlExit)
MODULE_LICENSE("GPL");
