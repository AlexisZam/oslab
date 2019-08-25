/* ch6.c */

#include <unistd.h> // dup2, pipe

int main() {
    int pipefd1[2];
    int pipefd2[2];
    pipe(pipefd1);
    pipe(pipefd2);
    dup2(pipefd1[0], 33);
    dup2(pipefd1[1], 34);
    dup2(pipefd2[0], 53);
    dup2(pipefd2[1], 54);
    execl("riddle", "riddle", NULL);
}
