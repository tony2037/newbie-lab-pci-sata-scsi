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
#define VENDOR_ID 0x8086
#define DEVICE_ID 0x31e3
#define SUBSYS_VENDOR_ID 0x1b4b
#define SUBSYS_DEVICE_ID 0x9235
#define REQUIRE_BAR 5
#define SSTATUS_OFFSET 0x28
#define PORT_REGISTER_OFFSET(port_number) (0x100+port_number*0x80)

#define SLORT4_PORT 1

static struct pci_dev *dev = NULL;
static unsigned long io_base;
static unsigned long ports_base[4];
static unsigned int detection_state;

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

	detection_state = show_sstatus(ports_base[SLORT4_PORT], SSTATUS_OFFSET);
	sprintf(monitor_buf, "%u", detection_state);
	printk(KERN_INFO "Slot 4 detection state %u", detection_state);

	unsigned long ret;
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
	/* search for pci device through vendor id, device id, subsystem vendor id and subsystem device id */
	dev = pci_get_device(VENDOR_ID, DEVICE_ID, dev);
	if (dev == NULL) {
		printk(KERN_WARNING "Cannot found pci dev through: VENDOR_ID, DEVICE_ID, SUBSYS_VENDOR_ID, SUBSYS_DEVICE_ID\n");
	}

	ret = pci_enable_device(dev);
	if(ret < 0) 
		printk(KERN_WARNING "pci enable fail, vendor(%x):device(%x)\n", dev->vendor, dev->device);
	printk(KERN_WARNING "pci enable success, vendor(%x):device(%x)\n", dev->vendor, dev->device);

	/* Get the I/O base address from the appropriate base address register (bar) in the configuration space */
	io_base = pci_resource_start(dev, REQUIRE_BAR);
	for(i = 0; i < 4; i++)
		ports_base[i] = io_base + PORT_REGISTER_OFFSET(i);

	/* Mark this region as being spoken for */
	// ret = pci_request_region(dev, REQUIRE_BAR, "BAR 5 base address");
	// if(ret != 0)
	// 	printk(KERN_WARNING "require bar fail.\n");

	printk(KERN_INFO "io_base: %lx\n", io_base);
	for(i = 0; i < 4; i++) {
		printk(KERN_INFO "port%zu_base: %lx\n", i, ports_base[i]);
		ret = show_sstatus(ports_base[i], SSTATUS_OFFSET);
	}
	detection_state = show_sstatus(ports_base[SLORT4_PORT], SSTATUS_OFFSET);


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
	pci_disable_device(dev);
	pci_release_region(dev, REQUIRE_BAR);
	pci_dev_put(dev);
	printk(KERN_WARNING "pci uninstall\n");

	/* cdev */
	unregister_chrdev_region(pci_monitor_dev, num_of_dev);
	cdev_del(&pci_monitor_cdev);
}

module_init(pci_monitor_init)
module_exit(pci_monitor_exit)
MODULE_LICENSE("GPL");
