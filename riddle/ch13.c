/* ch13.c */

#include <fcntl.h>     // open
#include <sys/stat.h>  // open
#include <sys/types.h> // fork, ftruncate, open, wait
#include <sys/wait.h>  // wait
#include <unistd.h>    // execl, fork, ftruncate, write

int main() {
    int fd = open(".hello_there", O_WRONLY);
    pid_t pid = fork();
    if (pid == 0) {
        execl("riddle", "riddle", NULL);
    }
    sleep(1);
    ftruncate(fd, 32768);
}
