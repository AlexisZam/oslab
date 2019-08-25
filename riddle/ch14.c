/* ch14.c */

#include <stdlib.h>    // exit
#include <sys/types.h> // fork, wait
#include <sys/wait.h>  // wait
#include <unistd.h>    // execl, fork

int main() {
    pid_t pid;
    do {
        pid = fork();
        if (pid == 0) {
            pid = getpid();
            if (pid == 32767) {
                execl("riddle", "riddle", NULL);
            } else {
                exit(0);
            }
        }
        wait(NULL);
    } while (pid != 32767);
}
