#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>


#define BUFFER_SIZE 262144

char writeBuffer[BUFFER_SIZE];


int readFromPipe(int fd, char *buffer, char *prefix) {
	int rv;
	memset(writeBuffer, '\0', BUFFER_SIZE);

	if ( (rv = read(fd, buffer, BUFFER_SIZE)) < 0 ) {
		printf("READ ERROR FROM PIPE");
		return -1;
	} else if (rv == 0) {
		return -1;
	}

	int writeLen = 0;
	int dumpPrefix = 1;
	size_t len = strlen(buffer);

	for(int i = 0; i < len; i++) {
		char c = buffer[i];
		if(dumpPrefix == 1) {
			writeLen = strlen(prefix);
			memcpy(writeBuffer, prefix, writeLen);

			writeBuffer[writeLen] = ' ';
			writeLen++;

			dumpPrefix = 0;
		}
		if (c == '\n') {
			dumpPrefix = 1;
		}

		writeBuffer[writeLen] = c;
		writeLen++;
	}
	fprintf(stdout, "%s", writeBuffer);
	syslog (LOG_INFO, "%s", writeBuffer);

	return 0;
}

// if more than 10, fix all string allocations (currently it expects only one digit number)
int OUT_COUNT = 10;

int main (int argc, char **argv) {
	if(argc < 5) {
		printf(" usage: %s syslogTag appUuid appName.orderNum command arg1 arg2\n", argv[0]);
		return 1;
	}

	// syslog
	setlogmask (LOG_UPTO (LOG_INFO));
	openlog (argv[1], LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);

	int pipes[OUT_COUNT][2];

	for(int i=0; i < OUT_COUNT; i++) {
		pipe(pipes[i]);
	}


	if (fork() == 0) {
		for(int i=0; i < OUT_COUNT; i++) {
			dup2(pipes[i][1], i + 1);  // send stderr to the pipe
			close(pipes[i][1]);
		}

		execvp(argv[4], argv + 4);
		return 0;
	}

	int opened[OUT_COUNT];
	int openedCount = 0;
	char *prefixes[OUT_COUNT];
	struct timeval tv;
	int fdMax;
	fd_set readfds;


	for(int i=0; i < OUT_COUNT; i++) {
		close(pipes[i][1]);  // close the write end of the pipe in the parent
		opened[i] = 0;

		if(isatty(i+1) == 1) {
			opened[i] = 1;
			openedCount++;
		}

		prefixes[i] = malloc(strlen(argv[2]) + strlen(argv[3]) + 4);
		sprintf(prefixes[i], "%s %s %i", argv[2], argv[3], i + 1);
	}

	int numActive = 0;
	char mat[OUT_COUNT][BUFFER_SIZE];
	while(openedCount > 0) {
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
			printf("select() timed out\n");
			continue;
		}

		for(int i=0; i < OUT_COUNT; i++) {
			if(FD_ISSET(pipes[i][0], &readfds)) {
				openedCount += readFromPipe(pipes[i][0], mat[i], prefixes[i]);
			}
		}
	}
}
