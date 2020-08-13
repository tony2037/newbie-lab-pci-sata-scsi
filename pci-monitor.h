#include <linux/pci.h>

#define DET_NDNP 0x0
#define DET_PDNP 0x1
#define DET_PDEP 0x3
#define DET_OFFLINE 0x4

#define SIG_HOTPLUG 44

#define STATE_PLUGIN 1
#define STATE_UNPLUG 0
#define STATE_UNRECOGNIZE 2

struct controller {
	unsigned int vendor;	// vendor ID
	unsigned int device;	// device ID
	struct pci_dev *dev;
	unsigned long io_base;	// Get this with BAR (base address register 5)
};

struct ds_slot {
	struct controller *controller;
	int port_number;	// indicate which port the slot belongs to
	unsigned long port_base; // io_base + port register offset
	unsigned int detection_state;	// The detection state implies the state of the disk
};
