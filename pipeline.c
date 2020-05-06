/**
 * File: pipeline.c
 * ----------------
 * Presents the implementation of the pipeline routine.
 */

#include "pipeline.h"
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include "fork-utils.h"  // this has to be the last #include'd statement in the file

void pipeline(char *argv1[], char *argv2[], pid_t pids[]) {
  pid_t pid1, pid2;
  int fds[2];

  pipe(fds);

  pid1 = fork();
  if (pid1 == 0) {
    close(fds[0]);
    dup2(fds[1], STDOUT_FILENO);
    close(fds[1]);
    execvp(argv1[0], argv1);
  } else {
    pids[0] = pid1;
  }

  pid2 = fork();
  if (pid2 == 0) {
    close(fds[1]);
    dup2(fds[0], STDIN_FILENO);
    close(fds[0]);
    execvp(argv2[0], argv2);
  } else {
    pids[1] = pid2;
  }
  close(fds[0]);
  close(fds[1]);
}
