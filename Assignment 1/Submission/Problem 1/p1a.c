#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <n>\n", argv[0]);
        exit(1);
    }

    int n = atoi(argv[1]);

    printf("Main Process ID: %d, level: 0\n", getpid());
    

	for (int i=1;i<=n;i++){
		if (fork() == 0){
			printf("Process ID: %d, Parent ID: %d, level: %d\n", getpid(), getppid(), i);
		}
		else{
			wait(NULL);
		}	
	}

    return 0;
}

