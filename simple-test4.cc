/**
 * File: simple-test4.cc
 * ---------------------
 * Presents the implementation of a short nonsense program that makes a small number of
 * system calls.  The program can be run standalone, but it's really designed to be fed as
 * an argument to the trace executable, as with:
 * 
 *    > ./trace ./simple-test4
 *
 * This particular example is used to demonstrate that trace follows execution of the
 * parent and not the child in a fork() scenario.  The sleep(1) call in the child
 * is in place to make it virtually impossible for a SIGCHLD signal to be sent to the
 * parent before it exits.
 */

#include <unistd.h>

int main(int argc, char *argv[]) {
  getpid();
  write(STDOUT_FILENO, "before\n", 7);
  if (fork() == 0) { sleep(1); return 0; }
  getppid();
  write(STDOUT_FILENO, "after\n", 6);
  return 0;
}
