#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define MAX_CHILDREN 100
#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <number of children> <command>\n", argv[0]);
        return 1;
    }

    int n_children = atoi(argv[1]);
    if (n_children <= 0 || n_children > MAX_CHILDREN) {
        fprintf(stderr, "Number of children must be between 1 and %d\n", MAX_CHILDREN);
        return 1;
    }

    char *command = argv[2];

    // Create pipes for communication between parent and child processes
    int pipes[n_children][2];
    for (int i = 0; i < n_children; i++) {
        if (pipe(pipes[i]) == -1) {
            fprintf(stderr, "Failed to create pipe: %s\n", strerror(errno));
            return 1;
        }
    }

    // Fork child processes
    for (int i = 0; i < n_children; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            fprintf(stderr, "Failed to fork child process: %s\n", strerror(errno));
            return 1;
        } else if (pid == 0) {
            // Child process
            struct timeval start, end;
            gettimeofday(&start, NULL);

            pid_t pid2 = fork();
            if (pid2 == -1) {
                fprintf(stderr, "Failed to fork grandchild process: %s\n", strerror(errno));
                return 1;
            } else if (pid2 == 0) {
                // Grandchild process
                // Redirect stdout and stderr to /dev/null
                freopen("/dev/null", "w", stdout);
                freopen("/dev/null", "w", stderr);
                // Execute command
                execlp(command, command, NULL);
                fprintf(stderr, "Failed to execute command: %s\n", strerror(errno));
                exit(1);
            } else {
                // Child process
                int status;
                waitpid(pid2, &status, 0);
                gettimeofday(&end, NULL);
                double elapsed_time = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_usec - start.tv_usec) / 1000.0;
                // Send elapsed time back to parent process
                char buffer[BUFFER_SIZE];
                int len = snprintf(buffer, BUFFER_SIZE, "%.2f", elapsed_time);
                if (write(pipes[i][1], buffer, len) == -1) {
                    fprintf(stderr, "Failed to write to pipe: %s\n", strerror(errno));
                    return 1;
                }
                close(pipes[i][0]);
                close(pipes[i][1]);
                exit(0);
            }
        }
    }

    // Parent process
    double max_time = 0, min_time = -1, total_time = 0;
    int n_finished = 0;
    for (int i = 0; i < n_children; i++) {
        char buffer[BUFFER_SIZE];
        int len = read(pipes[i][0], buffer, BUFFER_SIZE);
        if (len == -1) {
            fprintf(stderr, "Failed to read from pipe: %s\n", strerror(errno));
            return 1;
        }
        buffer[len] = '\0';
        double elapsed_time = atof(buffer);
		if (min_time == -1 || elapsed_time < min_time) {
			min_time = elapsed_time;
		}
		if (elapsed_time > max_time) {
			max_time = elapsed_time;
		}
			total_time += elapsed_time;
			printf("Child %d Executed in %.2f millis\n", i + 1, elapsed_time);
			close(pipes[i][0]);
			close(pipes[i][1]);
			n_finished++;
	}
	
	// Wait for all child processes to finish
	while (n_finished < n_children) {
		int status;
		wait(&status);
		n_finished++;
	}

	printf("Max: %.2f millis\n", max_time);
	printf("Min: %.2f millis\n", min_time);
	printf("Average: %.2f millis\n", total_time / n_children);

	return 0;

}

