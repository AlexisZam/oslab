/* ch10.c */

#include <fcntl.h>     // creat, open
#include <string.h>    // strlen
#include <sys/stat.h>  // creat, open
#include <sys/types.h> // creat, fork, open, wait
#include <sys/wait.h>  // wait
#include <unistd.h>    // execl, fork, read, sleep, write, unlink

int main() {
    int fd = open("secret_number", O_RDONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    pid_t pid = fork();
    if (pid == 0) {
        execl("riddle", "riddle", NULL);
    }
    sleep(1);
    char buf[4096];
    read(fd, buf, 4096);
    fd = creat("foo", S_IRWXU);
    write(fd, buf, strlen(buf));
    wait(NULL);
}
