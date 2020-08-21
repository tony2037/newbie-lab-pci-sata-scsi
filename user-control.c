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
		goto end;
	}

        char *chpDevFile = argv[2];
        char *operation = argv[3];
        char buf[10] = {0};
        int slot = 0;

        //union ioctl_arg cmd;
        int fd = -1, dev_fd = -1;
        int ret = -1;
        int num = 0;

	sscanf(argv[1], "%d", &slot);
	if (slot > 4 || slot < 2) {
		printf("The slot number has to be within 2 to 4\n");
		goto end;
	}

	fd = open("/dev/user-control", O_RDWR);
        if (fd == -1) {
            perror("open /dev/user-control failed.\n");
            goto end;
        }

	printf("Let slot %d goes %s\n", slot, operation);
	sprintf(buf, "%d %s", slot, operation);
	if(strcmp(operation, "up") != 0 && strcmp(operation, "down") != 0) {
		printf("No such operation: %s\n", operation);
		goto end;
	}
	if(strcmp(operation, "down") == 0) {
                dev_fd = open(chpDevFile, O_RDWR);
                if (dev_fd == -1) {
                    printf("open %s failed.\n", chpDevFile);
                    goto end;
                }
		ARGS_STANDBYNOW(args_standbynow);
		ARGS_CHECKPOWERMODE(args_checkpowermode);

		ret = ioctl(dev_fd, HDIO_DRIVE_CMD, args_standbynow);
		if (ret == -1) {
			printf("ioctl standby immediately failed\n");
			goto end;
		}
		ret = ioctl(dev_fd, HDIO_DRIVE_CMD, args_checkpowermode);
		if (ret == -1) {
			printf("ioctl standby immediately failed\n");
			goto end;
		}

		switch (args_checkpowermode[2]) {
			case 0x00:
				printf("The disk is in standby mode\n");
				break;
			case 0x40:
				printf("The disk is in NVcache_spindown mode\n");
				goto end;
				break;
			case 0x41:
				printf("The disk is in NVcache_spinup mode\n");
				goto end;
				break;
			case 0x80:
				printf("The disk is in idle mode\n");
				goto end;
				break;
			case 0xff:
				printf("The disk is in active/idle mode\n");
				goto end;
				break;
			default:
				printf("Unknown state\n");
				goto end;
				break;
		}
	}

	if(write(fd, buf, strlen(buf) + 1) < 0) {
		perror("fwrite");
                ret = -1;
		goto end;
	}

	ret = close(fd);
	if (ret != 0) {
            perror("close\n");
            ret = -1;
            goto end;
        }

	ret = 0;

end:
        if (fd != -1) {
            close(fd);
        }
        if (dev_fd != -1) {
            close(dev_fd);
        }
        return ret;
}
