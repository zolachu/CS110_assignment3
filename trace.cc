/**
 * File: trace.cc
 * ----------------
 * Presents the implementation of the trace program, which traces the execution of another
 * program and prints out information about ever single system call it makes.  For each system call,
 * trace prints:
 *
 *    + the name of the system call,
 *    + the values of all of its arguments, and
 *    + the system calls return value
 */

#include <cassert>
#include <iostream>
#include <map>
#include <set>
#include <unistd.h> // for fork, execvp
#include <string.h> // for memchr, strerror
#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/wait.h>
#include "trace-options.h"
#include "trace-error-constants.h"
#include "trace-system-calls.h"
#include "trace-exception.h"
#include "fork-utils.h" // this has to be the last #include statement in this file
using namespace std;

std::map <int, string> systemCallNumbers;
std::map <string, int> systemCallNames;
std::map<string, systemCallSignature> systemCallSignatures;
static std::map<int, std::string> errorConstants;
static int registers[] = {RDI, RSI, RDX, R10, R8, R9};

static string readString(pid_t pid, unsigned long addr) {
  string str; 
  size_t numBytesRead = 0;
  while (true) {
    long ret = ptrace(PTRACE_PEEKDATA, pid, addr + numBytesRead);
    const char* forward = reinterpret_cast<char * const>(&ret);
    unsigned i;
    numBytesRead += sizeof(long);
    for (i = 0; i < sizeof(long) && *(forward + i) != '\0'; i++) str += *(forward + i);
    if (i != sizeof(long)) return str;
  }

  return str;
}

void enterSysCall(pid_t pid, long& retval, bool simple) {

  int syscall = ptrace(PTRACE_PEEKUSER, pid, ORIG_RAX * sizeof(long));
  if (simple) {
    cout <<  "syscall(" << syscall << ") ";
  } else {
    string sysCallNumber = systemCallNumbers[syscall];
    cout << sysCallNumber << "(";
    std::vector<scParamType> signatures =  systemCallSignatures[sysCallNumber];
    std::vector<scParamType>::iterator iter;
    for (iter = signatures.begin(); iter != signatures.end(); iter++) {
      unsigned index = iter - signatures.begin();
      enum scParamType signature = *iter;
      assert(signature != SYSCALL_UNKNOWN_TYPE);

      switch(signature) {

      case SYSCALL_INTEGER:
	cout << ptrace(PTRACE_PEEKUSER, pid, registers[index] * sizeof(long));
	break;
      case SYSCALL_STRING: {
	unsigned long retval = ptrace(PTRACE_PEEKUSER, pid, registers[index] * sizeof(long));
	string str = readString(pid, retval);
	cout << "\"" << str << "\"";
	break;
      }
      default: { 
	long retval = ptrace(PTRACE_PEEKUSER, pid, registers[index] * sizeof(long));
	(retval == 0) ? (cout << "NULL") :  (cout << "0x" << std::hex << retval << std::dec);
	break;
      }

      }
      (iter != signatures.end() - 1) ? (cout << ", ") : (cout <<  ") ");
    }

    if(syscall == systemCallNames["exit_group"]) retval = ptrace(PTRACE_PEEKUSER, pid, registers[0] * sizeof(long));
  }
}

void exitSysCall(pid_t pid, bool simple) {
  long retval = ptrace(PTRACE_PEEKUSER, pid, RAX * sizeof(long));
  cout << "= ";
  if (simple) {
    cout << retval << endl;
    return;
  }
  if (retval < 0) {
    cout << "-1"  << " " << errorConstants[abs(retval)] << " (" << strerror(abs(retval)) << ")"  << endl;
    return;
  }

  if (retval == 0) {
    cout << "0" << endl;
    return;
  }
  //  if(retval <= INT_MAX){
  //  cout << retval << endl;
  //  return;
  // }
  cout << "0x" << std::hex << retval<<  std::dec << endl;
}


int main(int argc, char *argv[]) {
  bool simple = false, rebuild = false;
  int numFlags = processCommandLineFlags(simple, rebuild, argv);
  if (argc - numFlags == 1) {
    cout << "Nothing to trace... exiting." << endl;
    return 0;
  }

  compileSystemCallData(systemCallNumbers, systemCallNames, systemCallSignatures, rebuild);

  try {
    compileSystemCallErrorStrings(errorConstants);
  } catch (MissingFileException& e) {
    std::cout << e.what() << endl;
  }
  
  pid_t pid = fork();
  if (pid == 0) {
    ptrace(PTRACE_TRACEME);
    raise(SIGSTOP);
    execvp(argv[numFlags + 1], argv + numFlags + 1);
    return 0;
  }

  int status;
  waitpid(pid, &status, 0);
  assert(WIFSTOPPED(status));
  ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_TRACESYSGOOD);

  ptrace(PTRACE_SYSCALL, pid, 0, 0);
  
  long retval = 0;
  while (true) {
    //    ptrace(PTRACE_SYSCALL, pid, 0, 0);
    int status;
    waitpid(pid, &status, 0);

    if(WIFEXITED(status) || WIFSIGNALED(status)) break;

    if(WIFSTOPPED(status) && (WSTOPSIG(status) == (SIGTRAP|0x80))) {
	enterSysCall(pid, retval, simple);
	ptrace(PTRACE_SYSCALL, pid, 0, 0);

	int status1;
	waitpid(pid, &status1, WUNTRACED);

	ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_TRACESYSGOOD);

	if(WIFEXITED(status1) || WIFSIGNALED(status1)) break;

	if(WIFSTOPPED(status1) && (WSTOPSIG(status1) == (SIGTRAP|0x80))) {
	  exitSysCall(pid, simple);
	  ptrace(PTRACE_SYSCALL, pid, 0, 0);
	}
      }
      ptrace(PTRACE_SYSCALL, pid, 0, 0);
  }
  cout << "= <no return>" << endl;
  cout << "Program exited normally with status " << retval << endl;

  return 0;
}
