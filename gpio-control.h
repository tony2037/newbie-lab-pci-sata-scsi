#include <linux/ioctl.h>

/* According to  Documentation/ioctl/ioctl-number.txt pick a an unused block */
#define IOC_MAGIC '\x66'

#define IOCTL_UP _IOW(IOC_MAGIC, 0, int)
#define IOCTL_DOWN _IOW(IOC_MAGIC, 1, int)
#define IOCTL_GET _IOR(IOC_MAGIC, 2, int)
