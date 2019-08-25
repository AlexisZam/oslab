/* ch12.c */

#include <fcntl.h>     // open
#include <sys/stat.h>  // open
#include <sys/types.h> // lseek, open
#include <unistd.h>    // write

int main(int argc, char **argv) {
    char *pathname = argv[1];
    char *buf = argv[2];
    int fd = open(pathname, O_WRONLY);
    lseek(fd, 0x6f, SEEK_SET);
    write(fd, buf, 1);
}
