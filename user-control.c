#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "gpio-control.h"

#define HDIO_DRIVE_CMD 0x031f
#define ATA_OP_STANDBYNOW 0xe0
#define ATA_OP_CHECKPOWERMODE 0xe5
#define ARGS_STANDBYNOW(x) uint8_t x[4] = {ATA_OP_STANDBYNOW,0,0,0};
#define ARGS_CHECKPOWERMODE(x) uint8_t x[4] = {ATA_OP_CHECKPOWERMODE,0,0,0};

int main(int argc, char **argv) {
	if (argc < 3) {
		printf("Usage: ./user-control SLOT_NUMBER{2-4} DEVICE_FILE OPERATION{up/down}\n");
		printf("E.g: ./user-control 4 /dev/sata1 up\n");
		printf("Make sure do: mknod /dev/user-control c {major number} 0\n");
		return -1;
	}

        char *dev_file = argv[2];
        char *operation = argv[3];
        char buf[10];
        char sg_raw_buf[128];
	int slot;
	sscanf(argv[1], "%d", &slot);
	if (slot > 4 || slot < 2) {
		printf("The slot number has to be within 2 to 4\n");
		return -1;
	}

	//union ioctl_arg cmd;
        int fd, dev_fd;
        int ret;
	int close_ret;
 	int num = 0;

	fd = open("/dev/user-control", O_RDWR);
        if (fd == -1) {
            perror("open /dev/user-control failed.\n");
            return -1;
        }

	printf("Let slot %d goes %s\n", slot, operation);
	sprintf(buf, "%d %s", slot, operation);
	if(strcmp(operation, "up") != 0 && strcmp(operation, "down") != 0) {
		printf("No such operation: %s\n", operation);
		return -1;
	}
	if(strcmp(operation, "down") == 0) {
                dev_fd = open(dev_file, O_RDWR);
                if (dev_fd == -1) {
                    printf("open %s failed.\n", dev_file);
                    return -1;
                }
		ARGS_STANDBYNOW(args_standbynow);
		ARGS_CHECKPOWERMODE(args_checkpowermode);

		ret = ioctl(dev_fd, HDIO_DRIVE_CMD, args_standbynow);
		if (ret == -1) {
			printf("ioctl standby immediately failed\n");
			return -1;
		}
		ret = ioctl(dev_fd, HDIO_DRIVE_CMD, args_checkpowermode);
		if (ret == -1) {
			printf("ioctl standby immediately failed\n");
			return -1;
		}

		switch (args_checkpowermode[2]) {
			case 0x00:
				printf("The disk is in standby mode\n");
				break;
			case 0x40:
				printf("The disk is in NVcache_spindown mode\n");
				return -1;
				break;
			case 0x41:
				printf("The disk is in NVcache_spinup mode\n");
				return -1;
				break;
			case 0x80:
				printf("The disk is in idle mode\n");
				return -1;
				break;
			case 0xff:
				printf("The disk is in active/idle mode\n");
				return -1;
				break;
			default:
				printf("Unknown state\n");
				return -1;
				break;
		}
	}

	if(write(fd, buf, strlen(buf) + 1) < 0) {
		perror("fwrite");
		return -1;
	}

	close_ret = close(fd);
	if (close_ret != 0) perror("close\n");

	return 0;
}
