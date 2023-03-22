#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main() {
    pid_t pid = fork();

    if (pid < 0) {
        printf("Fork failed.\n");
        exit(1);
    } else if (pid == 0) {
        // Child process
        exit(0);
    } else {
        // Parent process
        sleep(5);
        // Wait for the child to exit
        wait(NULL);
    }

    return 0;
}


