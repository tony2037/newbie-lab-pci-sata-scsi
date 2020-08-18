#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "gpio-control.h"

#define SG_RAW_STANDBY_IMMEDIATE_FORMAT "sg_raw /dev/sata%d A1 06 00 00 00 00 00 00 00 E0 00 00"

int main(int argc, char **argv) {
	if (argc < 3) {
		printf("Usage: ./user-control SLOT_NUMBER{2-4} OPERATION{up/down}\n");
		printf("E.g: ./user-control 4 up\n");
		printf("Make sure do: mknod /dev/user-control c {major number} 0\n");
		return -1;
	}

	char *operation = argv[2];
	char buf[10];
	char sg_raw_buf[128];
	int slot;
	sscanf(argv[1], "%d", &slot);
	if (slot > 4 || slot < 2) {
		printf("The slot number has to be within 2 to 4\n");
		return -1;
	}

	//union ioctl_arg cmd;
        int fd;
 	long ret;
	int close_ret;
 	int num = 0;

	fd = open("/dev/user-control", O_RDWR);
        if (fd == -1) perror("open");

	//memset(&cmd, 0, sizeof(cmd));

	printf("Let slot %d goes %s\n", slot, operation);
	sprintf(buf, "%d %s", slot, operation);
	if(strcmp(operation, "up") != 0 && strcmp(operation, "down") != 0) {
		printf("No such operation: %s\n", operation);
		return -1;
	}
	if(strcmp(operation, "down") == 0) {
		int i = 1;
		for(i = 1; i < 5; i++) {
			memset(sg_raw_buf, 0, 128);
			sprintf(sg_raw_buf, SG_RAW_STANDBY_IMMEDIATE_FORMAT, i);
			system(sg_raw_buf);
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
