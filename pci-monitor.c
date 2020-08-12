#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include <asm/uaccess.h>
#include <linux/pci.h>
#include <linux/io.h>

#include "pci-monitor.h"

#define DRIVER_NAME "pci-monitor"
#define VENDOR_ID 0x1b4b
#define DEVICE_ID 0x9235
#define SUBSYS_VENDOR_ID 0x1b4b
#define SUBSYS_DEVICE_ID 0x9235
#define REQUIRE_BAR 5
#define SSTATUS_OFFSET 0x28
#define PORT_REGISTER_OFFSET(port_number) (0x100+port_number*0x80)

struct pci_dev *dev = NULL;
unsigned long io_base;
unsigned long port0_base;
unsigned long port1_base;

void show_sstatus(unsigned long port_register_base, unsigned long offset) {
	unsigned int *reg = ioremap(port_register_base + offset, 4);
	unsigned int register_data = *reg;
	unsigned int det = register_data & 0xf; // 0:3 device detection
	printk(KERN_INFO "sstatus: %x\n", register_data);

	switch(det) {
		case 0x0:
			printk(KERN_INFO "No device detected and Phy communication not established\n");
			break;
		case 0x1:
			printk(KERN_INFO "Device presence detected but Phy communication not established\n");
			break;
		case 0x3:
			printk(KERN_INFO "Device presence detected and Phy communication established\n");
			break;
		case 0x4:
			printk(KERN_INFO "Phy in offline mode as a result of the interface being disabled or running in a BIST loopback mode\n");
			break;
		default:
			printk(KERN_INFO "Unrecognize device detection\n");
			break;
	}
}

static int __init pci_monitor_init(void)
{
	int ret = 0;
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
	port0_base = io_base + PORT_REGISTER_OFFSET(0);
	port1_base = io_base + PORT_REGISTER_OFFSET(1);

	/* Mark this region as being spoken for */
	// ret = pci_request_region(dev, REQUIRE_BAR, "BAR 5 base address");
	// if(ret != 0)
	// 	printk(KERN_WARNING "require bar fail.\n");

	printk(KERN_INFO "io_base: %lx\n", io_base);
	printk(KERN_INFO "port0_base: %lx\n", port0_base);
	printk(KERN_INFO "port1_base: %lx\n", port1_base);
	show_sstatus(port0_base, SSTATUS_OFFSET);
	show_sstatus(port1_base, SSTATUS_OFFSET);

	return 0;
}

static void __exit pci_monitor_exit(void)
{
	pci_disable_device(dev);
	pci_release_region(dev, REQUIRE_BAR);
	pci_dev_put(dev);
	printk(KERN_WARNING "pci uninstall\n");
}

module_init(pci_monitor_init)
module_exit(pci_monitor_exit)
MODULE_LICENSE("GPL");
