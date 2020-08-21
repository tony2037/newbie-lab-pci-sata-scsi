#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "pci-monitor.h"

#define SLOT_NUM 4
static unsigned int detection_states[SLOT_NUM + 1];
static char buf[10];
static unsigned int tmp_states[SLOT_NUM + 1];

void receiveData(int n, siginfo_t *info, void *unused)
{
	int state = info->si_int;
	printf("received value %i\n", state);

	if(state == STATE_PLUGIN) {
		system("synodsmnotify admin Detect_PLUGIN Detect_PLUGIN");
	} else if (state == STATE_UNPLUG) {
		system("synodsmnotify admin Detect_UNPLUG Detect_UNPLUG");
	} else {
		system("synodsmnotify admin Detect_UNRECOGNIZE Detect_UNRECOGNIZE");
	}
}

void check_state() {
	size_t i = 1;
	for (i = 1; i < SLOT_NUM + 1; i++) {
		if(tmp_states[i] != detection_states[i]) {
			detection_states[i] = tmp_states[i];
			char message[128];
			switch(detection_states[i]) {
				case DET_NDNP:
					sprintf(message, "synodsmnotify admin SLOT[%zu] Detect_UNPLUG", i);
					break;
				case DET_PDEP:
					sprintf(message, "synodsmnotify admin SLOT[%zu] Detect_PLUGIN", i);
					break;
				default:
					sprintf(message, "synodsmnotify admin SLOT[%zu] Detect_UNRECOGNIZE", i);
					break;
			}
			system(message);
		}
	}
}

int main(int argc, char **argv) {
	int fd;

	fd = open("/dev/pci-receiver", O_RDWR);
        if (fd < 0) {
		printf("Use: mknod /dev/pci-reciver c {major number} 0 to create a cdev file");
		perror("open");
	}
	/* setup the signal handler for SIG_TEST 
	 * SA_SIGINFO -> we want the signal handler function with 3 arguments
	 */
	struct sigaction sig;
	sig.sa_sigaction = receiveData;
	sig.sa_flags = SA_SIGINFO;
	sigaction(SIG_HOTPLUG, &sig, NULL);

	pid_t pid, sid;
        
        /* Fork off the parent process */
        pid = fork();
        if (pid < 0) {
                exit(EXIT_FAILURE);
        }
	printf("pci receiver PID: %ld \n", pid);
        /* If we got a good PID, then
           we can exit the parent process. */
        if (pid > 0) {
                exit(EXIT_SUCCESS);
        }

        /* Change the file mode mask */
        umask(0);       
        
        /* Open any logs here */
        
        /* Create a new SID for the child process */
        sid = setsid();
        if (sid < 0) {
                /* Log any failures here */
                exit(EXIT_FAILURE);
        }
	printf("pci receiver SID: %ld \n", sid);
	memset(buf, 0, 10);
	sprintf(buf, "%i", sid);
	if (write(fd, buf, strlen(buf) + 1) < 0) {
		perror("fwrite"); 
		return -1;
	}
	memset(buf, 0, 10);
	if (read(fd, buf, 10) < 0) {
		perror("fread"); 
		return -1;
	}
	sscanf(buf, "%u %u %u %u", &detection_states[1], &detection_states[2], &detection_states[3], &detection_states[4]);
        
        /* Change the current working directory */
        if ((chdir("/")) < 0) {
                /* Log any failures here */
                exit(EXIT_FAILURE);
        }
        
        /* Close out the standard file descriptors */
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        
        /* Daemon-specific initialization goes here */
        
        /* The Big Loop */
        while (1) {
           /* Do some task here ... */
		memset(buf, 0, 10);
		if(read(fd, buf, 10) < 0) {
			perror("fread"); 
			return -1;
		}
		sscanf(buf, "%u %u %u %u", &tmp_states[1], &tmp_states[2], &tmp_states[3], &tmp_states[4]);
		check_state();
		sleep(1); /* wait 1 seconds */
        }
}
