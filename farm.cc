#include <cassert>
#include <ctime>
#include <cctype>
#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>
#include <sched.h>
#include "subprocess.h"
#include "fork-utils.h"  // this has to be the last #include'd statement in the file

using namespace std;

struct worker {
  worker() {}
  worker(char *argv[]) : sp(subprocess(argv, true, false)), available(false) {}
  subprocess_t sp;
  bool available;
};

static const size_t kNumCPUs = sysconf(_SC_NPROCESSORS_ONLN);
static vector<worker> workers(kNumCPUs);
static size_t numWorkersAvailable = 0;

static void markWorkersAsAvailable(int sig) {
  while(true) {
    pid_t pid = waitpid(-1, NULL, WNOHANG|WUNTRACED);
    if(pid <= 0)  break;
    for(size_t worker = 0; worker < kNumCPUs; worker++) {
      if(workers[worker].sp.pid == pid) {
	workers[worker].available = true;
	numWorkersAvailable++;
	break;
      }
    }
  }
}

static const char *kWorkerArguments[] = {"./factor.py", "--self-halting", NULL};
static void spawnAllWorkers() {
  cout << "There are this many CPUs: " << kNumCPUs << ", numbered 0 through " << kNumCPUs - 1 << "." << endl;
  for (size_t i = 0; i < kNumCPUs; i++) {
    cpu_set_t cpus;
    CPU_ZERO(&cpus);
    CPU_SET(i, &cpus);

    workers[i] = worker((char**)kWorkerArguments);
    sched_setaffinity(workers[i].sp.pid, sizeof(cpu_set_t), &cpus);
    cout << "Worker " << workers[i].sp.pid << " is set to run on CPU " << i << "." << endl;
  }
}

static size_t getAvailableWorker() {
  sigset_t additions, existingmask;
  sigemptyset(&additions);
  sigaddset(&additions, SIGCHLD);
  sigprocmask(SIG_BLOCK, &additions, &existingmask);
  while(numWorkersAvailable == 0) {
    sigsuspend(&existingmask);
  }
  int availableWorkers = kNumCPUs;
  for(size_t worker = 0; worker < kNumCPUs; worker++) {
    if(workers[worker].available) {
      availableWorkers = worker;
      break;
    }
  }

  sigprocmask(SIG_UNBLOCK, &additions, NULL);
  return availableWorkers;
}

static void broadcastNumbersToWorkers() {
  while(true) {
    string line;
    getline(cin, line);
    if (cin.fail()) break;
    size_t endpos;
    long long num = stoll(line, &endpos);
    if (endpos != line.size()) break;

    size_t numWorkers = getAvailableWorker();
    if(numWorkers < kNumCPUs) {
      workers[numWorkers].available = false;
      numWorkersAvailable--;
      dprintf(workers[numWorkers].sp.supplyfd, "%lld\n", num);
      kill(workers[numWorkers].sp.pid, SIGCONT);
    }
  }
}

static void waitForAllWorkers() {
  sigset_t additions, existingmask;
  sigemptyset(&additions);
  sigaddset(&additions, SIGCHLD);
  sigprocmask(SIG_BLOCK, &additions, &existingmask);
  while(numWorkersAvailable != kNumCPUs) {
    sigsuspend(&existingmask);
  }
  sigprocmask(SIG_UNBLOCK, &additions, NULL);
}

static void closeAllWorkers() {
  signal(SIGCHLD, SIG_DFL);

  for(size_t worker = 0; worker < kNumCPUs; worker++) {
    close(workers[worker].sp.supplyfd);
    kill(workers[worker].sp.pid, SIGCONT);
  }

  for(size_t i = 0; i < kNumCPUs; i++) {
    while(true) {
      int status;
      waitpid(workers[i].sp.pid, &status, 0);
      if(WIFEXITED(status)) break;
    }
  }
}

int main(int argc, char *argv[]) {
  signal(SIGCHLD, markWorkersAsAvailable);
  spawnAllWorkers();
  broadcastNumbersToWorkers();
  waitForAllWorkers();
  closeAllWorkers();
  return 0;
}
