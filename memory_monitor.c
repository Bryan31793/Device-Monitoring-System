#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <stdbool.h>

void mem_monitor();
void parser(const char *);

int main() {
    mem_monitor();

    return 0;
}

void mem_monitor() {
    int pipe1[2], pipe2[2];
    pipe(pipe1);   //create a pipe in the kernel
    pipe(pipe2);

    if(fork() == 0) {   //if child process
        close(pipe1[0]);   //close read channel
        dup2(pipe1[1], STDOUT_FILENO); //redirect STDOUT to the pipe's write end
        close(pipe1[1]);   //close write channel

        execlp("cat", "cat", "/proc/meminfo", NULL);    //execute cat /proc/meminfo
        perror("Process error");
        exit(1);
    }

    if(fork() == 0) {
        close(pipe2[0]);
        dup2(pipe2[1], STDOUT_FILENO);
        close(pipe2[1]);

        execlp("cat", "cat", "/proc/stat", NULL);
        perror("Process error");
        exit(1);
    }

    close(pipe1[1]);   //parent process closes write channel
    close(pipe2[1]);


    char buffer[1024];
    char line[2048];
    int line_len = 0;
    ssize_t n;

    while((n = read(pipe1[0], buffer, sizeof(buffer))) > 0) {
        for(int i = 0; i < n; i++) {
            if(buffer[i] == '\n') {
                line[line_len] = '\0';
                parser(line);    //call parser function
                line_len = 0;   //reset position
            }
            else {
                if(line_len < sizeof(line_len) - 1) 
                    line[line_len++] = buffer[i];
            }
        }
    }
    
    close(pipe1[0]);
    close(pipe2[0]);
    wait(NULL);
    wait(NULL);

}

void parser(const char *line) {
    char key[64];
    long value;
    char unit[16];

    if(sscanf(line, "%63[^:]: %ld %15s", key, &value, unit) == 3) {
        if(strcmp(key, "MemTotal") == 0 || strcmp(key, "MemFree") == 0)
            printf("%s =  %ld %s\n", key, value, unit);
    }
}