#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/select.h>



#define OUT_COUNT 10
#define BUFFER_SIZE 262144

char readBuffer[BUFFER_SIZE];
char writeBuffer[OUT_COUNT][BUFFER_SIZE];
char writeLen[OUT_COUNT];


void readFromPipe(int index, int fd, char *prefix) {
	int rv;

	if((rv = read(fd, readBuffer, BUFFER_SIZE - strlen(prefix) - 2)) < 0) {
		printf("READ ERROR FROM PIPE");
		return;
	} else if (rv == 0) {
		return;
	}

	for(int i = 0; i < rv; i++) {
		if (writeLen[index] == 0) {
			writeLen[index] = strlen(prefix);
			memcpy(writeBuffer[index], prefix, writeLen[index]);
			writeBuffer[index][writeLen[index]++] = ' ';
		}

		char c = readBuffer[i];

		if (c != '\n') {
			writeBuffer[index][writeLen[index]++] = c;
		}

		if (c != '\n' && writeLen[index] < BUFFER_SIZE - 1) {
			continue;
		}

		writeBuffer[index][writeLen[index]] = '\0';
		fprintf(stdout, "%s\n", writeBuffer[index]);
		fflush(stdout);
		syslog (LOG_INFO, "%s\n", writeBuffer[index]);
		writeLen[index] = 0;
	}

	return;
}

int main (int argc, char **argv) {
	if(argc < 4) {
		printf(" usage: %s syslogTag \"prefix\" command arg1 arg2 ..\n", argv[0]);
		return 1;
	}


	int pipes[OUT_COUNT][2];

	for(int i=0; i < OUT_COUNT; i++) {
		if (pipe(pipes[i]) < 0) {
			perror("Failed to create pipe\n");
			exit(1);
		}
	}


	int pid;
	if ((pid = fork()) == 0) {
		for(int i=0; i < OUT_COUNT; i++) {
			dup2(pipes[i][1], i + 1);  // send stderr to the pipe
			close(pipes[i][1]);
		}

		execvp(argv[3], argv + 3);
		return 0;
	}

	char *prefixes[OUT_COUNT];
	struct timeval tv;
	int fdMax;
	fd_set readfds;

	// syslog
	setlogmask (LOG_UPTO (LOG_INFO));
	openlog (argv[1], LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);

	for(int i=0; i < OUT_COUNT; i++) {
		close(pipes[i][1]);  // close the write end of the pipe in the parent

		prefixes[i] = malloc(strlen(argv[2]) + 4);
		sprintf(prefixes[i], "%s %i", argv[2], i + 1);
	}

	int status;
	int numActive = 0;

	while(waitpid (pid, &status, WNOHANG) == 0) {
		fdMax = 0;
		FD_ZERO(&readfds);

		for(int i=0; i < OUT_COUNT; i++) {
			FD_SET(pipes[i][0], &readfds);
			if (pipes[i][0] > fdMax) {
				fdMax = pipes[i][0];
			}
		}

		tv.tv_sec = 10;
		tv.tv_usec = 0;

		if ((numActive = select(fdMax + 1, &readfds, NULL, NULL, &tv)) == -1) {
			perror("select");
			exit(1);
		} else if (numActive == 0) {
			//printf("select() timed out\n");
			continue;
		}

		for(int i=0; i < OUT_COUNT; i++) {
			if(FD_ISSET(pipes[i][0], &readfds)) {
				readFromPipe(i, pipes[i][0], prefixes[i]);
			}
		}
	}
}
