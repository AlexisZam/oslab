/* ch8.c */

#include <fcntl.h>     // creat
#include <stdio.h>     // sprintf
#include <sys/stat.h>  // creat
#include <sys/types.h> // lseek, creat
#include <unistd.h>    // write

int main() {
    for (int i = 0; i < 10; i++) {
        char pathname[5];
        sprintf(pathname, "bf0%d", i);
        int fd = creat(pathname, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        lseek(fd, 1073741824, SEEK_SET);
        write(fd, "abcdefghijklmnop", 16);
    }
}
