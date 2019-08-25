/* ch5.c */

#include <unistd.h> // dup2, execl

int main() {
    dup2(0, 99);
    execl("riddle", "riddle", NULL);
}
