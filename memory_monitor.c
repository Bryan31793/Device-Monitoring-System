#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <stdbool.h>

void father_process();
void mem_monitor(int *);
void process_pipe(int *);
void parser(const char *);

int main() {
    father_process();
    return 0;
}

void father_process() {
    int pipe1[2], pipe2[2], pipe3[2];
    pipe(pipe1);
    pipe(pipe2);
    pipe(pipe3);

    if(fork() == 0) {
        mem_monitor(pipe1);
    }

    close(pipe1[1]);   //parent process closes write channel
    process_pipe(pipe1);
    close(pipe1[0]);
    
    wait(NULL);

}

void mem_monitor(int *pipe_fd) {
    
    close(pipe_fd[0]);   //close read channel
    dup2(pipe_fd[1], STDOUT_FILENO); //redirect STDOUT to the pipe's write end
    close(pipe_fd[1]);   //close write channel

    execlp("cat", "cat", "/proc/meminfo", NULL);    //execute cat /proc/meminfo
    perror("Process error");
    exit(1);
    
}

void process_pipe(int *pipe_fd) {
    char buffer[1024];
    char line[2048];
    int line_len = 0;
    ssize_t n;

    while((n = read(pipe_fd[0], buffer, sizeof(buffer))) > 0) {
        for(int i = 0; i < n; i++) {
            if(buffer[i] == '\n') {
                line[line_len] = '\0';
                parser(line);    //call parser function
                line_len = 0;   //reset position
            }
            else {
                if(line_len < sizeof(line) - 1) 
                    line[line_len++] = buffer[i];
            }
        }
    }
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