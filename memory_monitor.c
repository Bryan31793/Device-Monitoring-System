#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

void mem_monitor();

int main() {
    mem_monitor();

    return 0;
}

void mem_monitor() {
    int fd[2];
    pipe(fd);   //create a pipe in the kernel

    if(fork() == 0) {   //if child process
        close(fd[0]);   //close read channel
        dup2(fd[1], STDOUT_FILENO); //redirect STDOUT to the pipe's write end
        close(fd[1]);   //close write channel

        execlp("cat", "cat", "/proc/meminfo", NULL);    //execute cat /proc/meminfo
        perror("Process error");
        exit(1);
    }

    close(fd[1]);   //parent process closes write channel


    char buffer[4096];
    ssize_t bytes;
    while((bytes = read(fd[0], buffer, sizeof(buffer)-1)) > 0) {
        buffer[bytes] = '\0';
        printf("%s", buffer);
    }
    
    close(fd[0]);
    wait(NULL);


}