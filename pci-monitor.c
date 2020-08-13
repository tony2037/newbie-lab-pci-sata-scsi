#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <asm/uaccess.h>
#include <linux/pci.h>
#include <linux/io.h>
#include <linux/init.h>
#include <asm/siginfo.h>  //siginfo
#include <linux/rcupdate.h>   //rcu_read_lock
#include <linux/sched.h>  //find_task_by_pid_type
#include <linux/debugfs.h>
#include <linux/delay.h>

#include "pci-monitor.h"

#define DRIVER_NAME "pci-monitor"

#define CONTROLLER1_VENDOR_ID 0x8086
#define CONTROLLER1_DEVICE_ID 0x31e3
#define CONTROLLER2_VENDOR_ID 0x1b4b
#define CONTROLLER2_DEVICE_ID 0x9235

#define REQUIRE_BAR 5
#define SSTATUS_OFFSET 0x28
#define PORT_REGISTER_OFFSET(port_number) (0x100+port_number*0x80)

#define SLOT2_PORT 2	// belongs to controller 2
#define SLOT3_PORT 0	// belongs to controller 1
#define SLOT4_PORT 1	// belongs to controller 1

static struct controller controller1 = {
	.vendor = CONTROLLER1_VENDOR_ID,
	.device = CONTROLLER1_DEVICE_ID,
	.dev = NULL,
	.io_base = 0x0,
};

static struct controller controller2 = {
	.vendor = CONTROLLER2_VENDOR_ID,
	.device = CONTROLLER2_DEVICE_ID,
	.dev = NULL,
	.io_base = 0x0,
};

static struct ds_slot slots[5]; // There are 4 slots, but since we dont mess up with slot 1, there are only 2 - 4

/* signal sending */
static int pid;
static struct siginfo info;
static struct task_struct *task;

/* character device */
static unsigned int pci_monitor_major = 0;
static unsigned int num_of_dev = 1;
static struct cdev pci_monitor_cdev;
static int ioctl_num = 0;
static dev_t pci_monitor_dev; 


/* pci functions*/
unsigned int show_sstatus(unsigned long port_register_base, unsigned long offset) {
	unsigned int *reg = ioremap(port_register_base + offset, 4);
	unsigned int register_data = *reg;
	unsigned int det = register_data & 0xf; // 0:3 device detection
	printk(KERN_INFO "sstatus: %x\n", register_data);

	switch(det) {
		case DET_NDNP:
			printk(KERN_INFO "No device detected and Phy communication not established\n");
			break;
		case DET_PDNP:
			printk(KERN_INFO "Device presence detected but Phy communication not established\n");
			break;
		case DET_PDEP:
			printk(KERN_INFO "Device presence detected and Phy communication established\n");
			break;
		case DET_OFFLINE:
			printk(KERN_INFO "Phy in offline mode as a result of the interface being disabled or running in a BIST loopback mode\n");
			break;
		default:
			printk(KERN_INFO "Unrecognize device detection\n");
			break;
	}

	return det;
}

/* ioctl functions */
static int pci_monitor_open(struct inode *inode, struct file *filp)
{
	printk(KERN_INFO "%s open \n", DRIVER_NAME);
	return 0;
}

static int pci_monitor_close(struct inode *inode, struct file *filp)
{
	printk(KERN_ALERT "%s call.\n", __func__);
	printk(KERN_INFO "%s release \n", DRIVER_NAME);	
	return 0;
}

void send_signal(struct siginfo *info, struct task_struct *task, int signal, int state) {
	info->si_signo = signal;
	info->si_int = state;
	int ret;
	ret = send_sig_info(signal, info, task);
	if(ret < 0) {
		printk(KERN_INFO "Error sending signal\n");
		return;
	}
}

static ssize_t pci_monitor_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
	char monitor_buf[10];
	size_t i;
	unsigned long ret;

	for(i = 2; i < 5; i++) {
		slots[i].detection_state = show_sstatus(slots[i].port_base, SSTATUS_OFFSET);
		printk(KERN_INFO "slot%zu state: %u\n", i, slots[i].detection_state);
	}
	sprintf(monitor_buf, "%u %u %u %u", 0, slots[2].detection_state,  slots[3].detection_state, slots[4].detection_state);

	ret = copy_to_user(buf, monitor_buf, strlen(monitor_buf) + 1);

	return count;
}

static ssize_t pci_monitor_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos){
	char monitor_buf[10];

	/* read the value from user space */
	if(count > 10)
		return -EINVAL;
	unsigned long ret;
	ret = copy_from_user(monitor_buf, buf, count);
	sscanf(monitor_buf, "%d", &pid);
	printk(KERN_INFO "pid = %d\n", pid);
 
	/* prepare the signal */
	memset(&info, 0, sizeof(struct siginfo));
	info.si_signo = SIG_HOTPLUG;
	info.si_code = SI_QUEUE;
	/* real time signals may have 32 bits of data. */
	info.si_int = 0;
 
	rcu_read_lock();
	/* find the task with that pid */
	task = pid_task(find_pid_ns(pid, &init_pid_ns), PIDTYPE_PID);  
	if(task == NULL){
		printk(KERN_INFO "No such pid = %d\n", pid);
		rcu_read_unlock();
		return -ENODEV;
	}
	rcu_read_unlock();

	return count;
}

/* ioctl operation */
static struct file_operations fops = {
 .owner = THIS_MODULE,
 .open = pci_monitor_open,
 .release = pci_monitor_close,
 .write = pci_monitor_write,
 .read = pci_monitor_read,
};

static int __init pci_monitor_init(void)
{
	int ret = 0;
	size_t i = 0;
	/* search for pci device through vendor id, device id*/
	controller1.dev = pci_get_device(controller1.vendor, controller1.device, controller1.dev);
	if (controller1.dev == NULL) {
		printk(KERN_WARNING "Cannot found pci dev (1) through: %x:%x\n", controller1.vendor, controller1.device);
		return -1;
	}
	controller2.dev = pci_get_device(controller2.vendor, controller2.device, controller2.dev);
	if (controller2.dev == NULL) {
		printk(KERN_WARNING "Cannot found pci dev (2) through: %x:%x\n", controller2.vendor, controller2.device);
		return -1;
	}

	// Enable pci device
	ret = pci_enable_device(controller1.dev);
	if(ret < 0) 
		printk(KERN_WARNING "pci enable fail, vendor(%x):device(%x)\n", controller1.vendor, controller1.device);
	printk(KERN_WARNING "pci (1) enable success, vendor(%x):device(%x)\n", controller1.vendor, controller1.device);

	ret = pci_enable_device(controller2.dev);
	if(ret < 0) 
		printk(KERN_WARNING "pci enable fail, vendor(%x):device(%x)\n", controller2.vendor, controller2.device);
	printk(KERN_WARNING "pci (2) enable success, vendor(%x):device(%x)\n", controller2.vendor, controller2.device);

	/* Get the I/O base address from the appropriate base address register (bar) in the configuration space */
	controller1.io_base = pci_resource_start(controller1.dev, REQUIRE_BAR);
	controller2.io_base = pci_resource_start(controller2.dev, REQUIRE_BAR);

	/* Assign which pci the slots belong to*/
	slots[2].controller = &controller2;
	slots[3].controller = &controller1;
	slots[4].controller = &controller1;
	slots[2].port_number = SLOT2_PORT;
	slots[3].port_number = SLOT3_PORT;
	slots[4].port_number = SLOT4_PORT;

	/* Get the port register base with port register offset */
	for(i = 2; i < 5; i++) {
		slots[i].port_base = slots[i].controller->io_base + PORT_REGISTER_OFFSET(slots[i].port_number);
	}

	for(i = 2; i < 5; i++) {
		printk(KERN_INFO "slot%zu port regiseter base: %lx\n", i, slots[i].port_base);
		slots[i].detection_state = show_sstatus(slots[i].port_base, SSTATUS_OFFSET);
	}
	for(i = 2; i < 5; i++) {
		printk(KERN_INFO "slot%zu detection state: %u\n", i, slots[i].detection_state);
	}

	/* register cdev */
	pci_monitor_dev = MKDEV(pci_monitor_major, 0);
	int alloc_ret = 0;
	int cdev_ret = 0;

	alloc_ret = alloc_chrdev_region(&pci_monitor_dev, 0, num_of_dev, DRIVER_NAME);
	if(alloc_ret < 0) {
		if (cdev_ret == 0) cdev_del(&pci_monitor_cdev);
		printk(KERN_ALERT "%s driver: alloc_chrdev_region error.\n", DRIVER_NAME);
	}

	pci_monitor_major = MAJOR(pci_monitor_dev);

	cdev_init(&pci_monitor_cdev, &fops);
	cdev_ret = cdev_add(&pci_monitor_cdev, pci_monitor_dev, num_of_dev);
	if(cdev_ret < 0) {
		if (alloc_ret == 0) unregister_chrdev_region(pci_monitor_dev, num_of_dev);
		printk(KERN_ALERT "%s driver: cdev_add error.\n", DRIVER_NAME);
	}
	printk(KERN_WARNING "%s driver(major: %d) installed.\n", DRIVER_NAME, pci_monitor_major);

	return 0;
}

static void __exit pci_monitor_exit(void)
{
	pci_disable_device(controller1.dev);
	pci_disable_device(controller2.dev);
	pci_release_region(controller1.dev, REQUIRE_BAR);
	pci_release_region(controller2.dev, REQUIRE_BAR);
	pci_dev_put(controller1.dev);
	pci_dev_put(controller2.dev);
	printk(KERN_WARNING "pci uninstall\n");

	/* cdev */
	unregister_chrdev_region(pci_monitor_dev, num_of_dev);
	cdev_del(&pci_monitor_cdev);
}

module_init(pci_monitor_init)
module_exit(pci_monitor_exit)
MODULE_LICENSE("GPL");
