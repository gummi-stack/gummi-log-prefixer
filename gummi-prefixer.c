#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

static int *pids;

static int BUFFER_SIZE = 262144;


int readFromPipe(int fd, char *buffer, char *prefix) {
	int rv;


    if ( (rv = read(fd, buffer, BUFFER_SIZE)) < 0 ) {
        printf("READ ERROR FROM PIPE");
		return 0;
    }
    else if (rv == 0) {
        return 0;
    }

	int dumpPrefix = 1;
	size_t len = strlen(buffer);

	for(int i = 0; i < len; i++) {
		char c = buffer[i];
		if(dumpPrefix == 1) {
			printf("%s ", prefix);
			dumpPrefix = 0;
		}
		if (c == '\n') {
			dumpPrefix = 1;
		}

		printf("%c", c);
	}

	return 1;
}

int OUT_COUNT = 10;

int main (int argc, char **argv) {
	if(argc < 4) {
		printf(" usage: %s 550e8400-e29b-41d4-a716-446655440000 parser.1 /path/to/mrdka arg1 arg2\n", argv[0]);
		return 1;
	}


	int pipes[OUT_COUNT][2];

	for(int i=0; i < OUT_COUNT; i++) {
		pipe(pipes[i]);
	}

	if (fork() == 0) {
		char *params[argc];
		memcpy(params, argv + 3, sizeof params);
		params[argc-1] = NULL;

		// dup2(pipefd[1], 1);  // send stdout to the pipe
		// dup2(pipefd2[1], 2);  // send stderr to the pipe
		for(int i=0; i < OUT_COUNT; i++) {
			dup2(pipes[i][1], i + 1);  // send stderr to the pipe
			close(pipes[i][1]);
		}

		execvp(params[0], params);
		return 0;

	}

	for(int i=0; i < OUT_COUNT; i++) {
		int r = isatty(i+1);

	}

	int opened[OUT_COUNT]; // = malloc(sizeof(int)*OUT_COUNT);

	for(int i=0; i < OUT_COUNT; i++) {
		close(pipes[i][1]);  // close the write end of the pipe in the parent
		opened[i] = 0;

		if(isatty(i+1) == 1) {
			opened[i] = 1;
		}

	}


	char *prefixes[OUT_COUNT];
	for(int i = 0;i < OUT_COUNT; i++ ) {
		prefixes[i] = malloc(strlen(argv[1]) + strlen(argv[2]) + 4);
		sprintf(prefixes[i], "%s %s %i", argv[1], argv[2], i);
	}




	char **mat = (char **)malloc(OUT_COUNT * sizeof(char*));
	for(int i = 0; i < OUT_COUNT; i++) {
		mat[i] = (char *)malloc(BUFFER_SIZE * sizeof(char));
	}




	int cont = 1;
	while (cont) {
		for(int i=0; i < OUT_COUNT; i++) {
			if(opened[i] == 1) {
				opened[i] = readFromPipe(pipes[i][0], mat[i], prefixes[i]);
			}
		}
		cont = 0;
		for(int i=0; i < OUT_COUNT; i++) {
			cont = cont || opened[i];
		}
		// printf("cxcx %i", cont);
		// opened[1] = readFromPipe(pipes[1][0], mat[1], prefix1);
		// opened[2] = readFromPipe(pipes[2][0], mat[2], prefix1);
	}
}
