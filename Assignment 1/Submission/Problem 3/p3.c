#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_NUMBERS 1000

//Function to search a number x in given range (start - end) of numbers array
int search(int *numbers, int start, int end, int x) {
    for (int i = start; i < end; i++) {
        if (numbers[i] == x) {  //if found then print on console and exit with code 0
            printf("Number found at index: %d\n", i);
            return 0;
        }
    }
    return 1;
}

int main(int argc, char *argv[]) {
	
	//There should be atleast three arguments
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <number> <n_children>\n", argv[0]);
        return 1;
    }
	
	//saving arguments to variables
    int x = atoi(argv[1]);
    int n_children = atoi(argv[2]);
		
    if (n_children <= 0) {
        fprintf(stderr, "Number of children must be positive\n");
        return 1;
    }

    int numbers[MAX_NUMBERS];
    int n_numbers = 0;

    // Read input sequence from stdin
    char buffer[100];
    while (fgets(buffer, 100, stdin) != NULL) {
        if (n_numbers == MAX_NUMBERS) {
            fprintf(stderr, "Too many numbers\n");
            return 1;
        }
        numbers[n_numbers++] = atoi(buffer);
    }
	
	//dividing the array among number of childre
    int chunk_size = n_numbers / n_children;
    int remainder = n_numbers % n_children;
	
	//assigning portion of array and creating children for searching
    for (int i = 0; i < n_children; i++) {
        int start = i * chunk_size;
        int end = start + chunk_size;
        if (i == n_children - 1) {
            end += remainder;
        }
        pid_t pid = fork();
        if (pid == -1) {
            fprintf(stderr, "Failed to fork child process %d\n", i);
            return 1;
        } else if (pid == 0) {
            // Child process
            int result = search(numbers, start, end, x);
            exit(result);
        }
    }

    // Parent process
    int status;
    pid_t pid;
    while ((pid = wait(&status)) != -1) {
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            // A child found the number, kill the other children and exit
            for (int i = 0; i < n_children; i++) {
                if (i != pid) {
                    kill(pid, SIGKILL);
                }
            }
            return 0;
        }
    }

    // None of the children found the number
    return 1;
}

