#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "gpio-control.h"

int main(int argc, char **argv) {
	if (argc < 3) {
		printf("Usage: ./user-control DEV_FILE OPERATION");
		return -1;
	}
	char *dev_file = argv[1];
	char *operation = argv[2];

	//union ioctl_arg cmd;
        int fd;
 	long ret;
	int close_ret;
 	int num = 0;

	fd = open(dev_file, O_RDWR);
        if (fd == -1) perror("open");

	//memset(&cmd, 0, sizeof(cmd));

	if(strcmp(operation, "up") == 0) {
        	ret = ioctl(fd, IOCTL_UP, &num);
		printf("Set gpio to up\n");	
	}
	else if(operation, "down" == 0) {
        	ret = ioctl(fd, IOCTL_DOWN, &num);
		printf("Set gpio to down\n");
	}
	else if(operation, "get" == 0) {
		int num = 0;
        	ret = ioctl(fd, IOCTL_GET, &num);
		printf("The value is %d\n", num);
	}

	close_ret = close(fd);
	if (close_ret != 0) perror("close\n");

	return 0;
}
