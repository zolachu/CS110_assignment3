/**
 * File: subprocess.cc
 * -------------------
 * Presents the implementation of the subprocess routine.
 */

#include "subprocess.h"
#include "fork-utils.h" // this has to be the very last #include statement in this .cc file!
using namespace std;

void try_pipe(int* fd);
void try_execvp(char* firstChar, char* argv[]);
void try_close(int fd);
void try_dup2(int fd, int fd1);


subprocess_t subprocess(char *argv[], bool supplyChildInput, bool ingestChildOutput) throw (SubprocessException) {

  int fds1[2];
  int fds2[2];
  pipe(fds1);
  pipe(fds2);
  subprocess_t sp = {fork(), kNotInUse, kNotInUse};

  if (sp.pid == 0){
    if (supplyChildInput) {
      try_dup2(fds1[0], STDIN_FILENO);
    }
    try_close(fds1[0]);
    try_close(fds1[1]);
    if (ingestChildOutput) {
      try_dup2(fds2[0], STDIN_FILENO);
    }
    try_close(fds2[0]);
    try_close(fds2[1]);

    try_execvp(argv[0], argv);
  } else {
    try_close(fds1[0]);
    try_close(fds2[1]);
    if (supplyChildInput) {
      sp.supplyfd = fds1[1];
    }
    if (ingestChildOutput) {
      sp.ingestfd =  fds2[0];
    }
  }
  return sp;
}

void try_pipe(int* fd) {
  if (pipe(fd) < -1) throw SubprocessException("failed to pipe");
}

void try_execvp(char* firstChar, char* argv[]) {
  if (execvp(firstChar, argv) < -1) throw SubprocessException("failed to execute the command");
}

void try_close(int fd) {
  if (close(fd) == -1) throw SubprocessException("failed to close the file descriptor");
}

void try_dup2(int fd, int fd1) {
  if (dup2(fd, fd1) == -1) throw SubprocessException("failed to duplicate the filedescriptor");
}
