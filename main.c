#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <signal.h>
#include "process_info.h"
#include "stats_functions.h"

#define MAX_STRING_LEN 4096

// argument handling
int setFlags(int*, int, char**);

// signals
void handleSignals(struct sigaction*, struct sigaction*);
void setChildrenSignalHandler(struct sigaction* sigint);
void tstpHandler();
void intHandler();

// handling processes
void handleProcesses(int*); 
ProcessInfo initProcess(ProcessInfo*, void (*func)(int*, int[2]), int* flags, 
                        int pipes[2], ProcessType, struct sigaction* sigint);
void addProcessToArray(ProcessInfo*, int, void (*func)(int*, int[2]), 
                       int* flags, int pipes[2], ProcessType, struct sigaction* sigint);

// extra stuff in main
void displayHeaderInfo(int samples, int sampleNumber, int timeDelay);
void displaySystemInformation();

// screen
void refreshScreen();

// help/error messages
void printHelpPage(char*);
void printErrorMessage(int, char*);

int main(int argc, char *argv[]) {
  
   int flags[6] = {
    0, //user
    0, //system
    0, //graphics
    0, //sequential
    10, //samples
    1, //tdelay seconds
  };

  if(setFlags(flags, argc, argv) == 0) {
    return 0;
  }

  handleProcesses(flags);

  return 0;

}

void tstpHandler() {
  // do nothing
}

void intHandler() {

  printf("Would you like to exit? yes (y) / no (any key): ");

  char answer;
  scanf(" %c", &answer);

  if (answer == 'y' || answer == 'Y') {
    exit(0);
  } 

}

void intHandlerChild() {
  // do nothing
}

void handleSignals(struct sigaction* tstp, struct sigaction* sigint) {

  tstp -> sa_handler = tstpHandler;
  sigfillset(&(tstp -> sa_mask)); // block signals
  tstp -> sa_flags = 0;
  
  if (sigaction(SIGTSTP, tstp, NULL) == -1) {
    perror("SIGTSTP in handleSignals");
  }

  sigint -> sa_handler = intHandler;
  sigfillset(&(sigint -> sa_mask));
  sigint -> sa_flags = 0;

  if (sigaction(SIGINT, sigint, NULL) == -1) {
    perror("SIGINT in handleSignals");
  }

}

void setChildrenSignalHandler(struct sigaction* sigint) {
 
  sigint -> sa_handler = intHandlerChild; // set handler to the new child handler
  sigemptyset(&(sigint -> sa_mask));

  if (sigaction(SIGINT, sigint, NULL) == -1) {
    perror("SIGINT in setChildrenSignalHandler");
  }

}

ProcessInfo initProcess(ProcessInfo *processes, void (*func)(int*, int[2]), int* flags, 
                        int pipes[2], ProcessType type, struct sigaction* sigint) {
  
  if (pipe(pipes) == -1) {
    
    perror("Error creating pipe in initProcess");

    ProcessInfo processInfo = {
      .success = false
    };
  
    return processInfo;

  }

  pid_t pid = fork();

  if (pid == -1) {

    close(pipes[0]);
    close(pipes[1]);

    perror("Error forking in initProcess");

    ProcessInfo processInfo = {
      .success = false
    };

    return processInfo;

  }

  if (pid == 0) {

    // close all previous pipes this one doesn't need
    for (int i = 0; i < 3; i++) {

      if (!processes[i].success) {
        continue;
      }

      close(processes[i].pipeAccess[0]);
      close(processes[i].pipeAccess[1]);

    }

    setChildrenSignalHandler(sigint); // set the signal handler for the children

    close(pipes[0]); // close read end for child, child doesn't need it

    // child

    (*func)(flags, pipes);

    ProcessInfo processInfo = {
      .success = true,
      .isChild = true,
      .processType = type,
      .pipeAccess = {pipes[0], pipes[1]}
    };

    return processInfo;

  } 

  // is parent
  
  ProcessInfo processInfo = {
    .pid = pid,
    .processType = type,
    .pipeAccess = {pipes[0], pipes[1]},
    .success = true,
    .isChild = false
  };

  return processInfo;

}

void addProcessToArray(ProcessInfo *processes, int index, void (*func)(int*, int[2]), 
                       int* flags, int pipes[2], ProcessType type, struct sigaction* sigint) {

  ProcessInfo processInfo = initProcess(processes, func, flags, pipes, type, sigint);

  if (!processInfo.success) {
    perror("Error making new user process in initProcess");
    return;
  }

  if (processInfo.isChild) {
    close(processInfo.pipeAccess[1]);
    exit(0);
    return;
  }

  close(processInfo.pipeAccess[1]); // close write end of the pipe, parent doesn't use it

  processes[index] = processInfo;

}

void handleProcesses(int* flags) {

  int user = flags[0];
  int system = flags[1];
  int sequential = flags[3];
  int samples = flags[4];
  int tdelay = flags[5];

  // children[0] is memory process, children[1] is user process, children[2] is cpu process. -1 if we don't have a new process for that
  ProcessInfo invalid = {
    .success = false
  };

  ProcessInfo processes[3] = {invalid, invalid, invalid};
  int pipes[3][2];

  ProcessType memoryType = 0;
  ProcessType userType = 1;
  ProcessType cpuType = 2;

  struct sigaction tstp;
  struct sigaction sigint;

  handleSignals(&tstp, &sigint);

  // init processes for memory, user, and cpu. using for loop to mitigate how often we repeat the code 
  if (user == 1) {
    addProcessToArray(processes, 1, handleReportUsers, flags, pipes[1], userType, &sigint);
  }

  if (system == 1) {
    addProcessToArray(processes, 0, handleReportMemory, flags, pipes[0], memoryType, &sigint);
    addProcessToArray(processes, 2, handleReportCPU, flags, pipes[2], cpuType, &sigint);
  }

  for (int i = 0; i < samples; i++) {

    if (sequential == 0) {
      refreshScreen();
    }

    bool printedHeader = false;

    for (int j = 0; j < 3; j++) {

      // make sure it is a valid process 
      if (!processes[j].success) {
        continue; 
      }

      // we loop from memory -> user -> cpu to ensure correct order
    
      char buffer[MAX_STRING_LEN];

      if (read(processes[j].pipeAccess[0], buffer, MAX_STRING_LEN) > 0) {

        // we do this so that formatting is correct
        if (!printedHeader) {
          displayHeaderInfo(samples, i + 1, tdelay);
          printedHeader = true;
        }

        printf("%s", buffer);

        // append system info after cpu info 
        if (processes[j].processType == cpuType) {
          displaySystemInformation();
        }

      }

    } 

  }

  int status = 0;

  // wait for children to finish
  while (wait(&status) > 0); 

  // close all pipe read fds
  for (int i = 0; i < 3; i++) {

    if (!processes[i].success) {
      continue;
    }

    close(processes[i].pipeAccess[0]);

  }

}

void displaySystemInformation() {

  printf("----------System-Information----------\n");

  // create the buffer
  struct utsname systemInfo;

  if (uname(&systemInfo) == -1) {
    printf("Error Fetching System Information... uname\n");
    return;
  }

  // multiple print statements for clarity
  printf("System Name: %s\n", systemInfo.sysname);
  printf("Machine Name: %s\n", systemInfo.nodename);
  printf("OS Release: %s\n", systemInfo.release);
  printf("OS Version: %s\n", systemInfo.version);
  printf("Architecture: %s\n", systemInfo.machine);


  printf("--------------------------------------\n");

}

void displayHeaderInfo(int samples, int sampleNumber, int timeDelay) {

  printf("\n+-------------------------------------+\n\n");

  printf("%d samples every %d second(s)\n", samples, timeDelay);
  printf("Sample #%d\n\n", sampleNumber);

  printf("Memory Usage: %d kB\n\n", getCurrentProcessUsage());

}

void refreshScreen() {
  // print escape codes
  printf("\033[0;0H"); //cursor to 0
  printf("\033[2J"); // refresh screen
}

int setFlags(int *flags, int argc, char *argv[]) {
  
  char *execName = argv[0];

  // parse command line arguments
  for (int i = 1; i < argc; i++) {

    // get the flag separated from =
    char *flag = strtok(argv[i], "=");

    if (strcmp(flag, "--help") == 0) {
      printHelpPage(execName);
      return 0;
    }

    if (strcmp(flag, "--user") == 0) {
      flags[0] = 1;
    } else if (strcmp(flag, "--system") == 0) {
      flags[1] = 1;
    } else if (strcmp(flag, "--graphics") == 0) {
      flags[2] = 1;
    } else if (strcmp(flag, "--sequential") == 0) {
      flags[3] = 1;
    } else if (strcmp(flag, "--samples") == 0) {

      // get the value after the =
      flag = strtok(NULL, "=");

      // make sure value is valid
      if (flag == NULL) {
        printErrorMessage(1, execName);
        return 0;
      }

      // convert str to int
      int sampleSize = strtol(flag, NULL, 10);

      // ensure value is valid
      if (sampleSize > 0) {
      
        flags[4] = sampleSize;
      
      } else {
        printErrorMessage(1, execName);
        return 0;
      }

    } else if (strcmp(flag, "--tdelay") == 0) {

      // similarly as above

      flag = strtok(NULL, "=");

      if (flag == NULL) {
        printErrorMessage(2, execName);
        return 0;
      }

      int timeDelay = strtol(flag, NULL, 10);

      if (timeDelay > 0) {

        flags[5] = timeDelay;

      } else {
        printErrorMessage(2, execName);
        return 0;
      }

    } else if (i == 1) {

      int samples = strtol(flag, NULL, 10);

      if (samples > 0) {

        flags[4] = samples;

      } else {
        printErrorMessage(1, execName);
        return 0;
      }

    } else if (i == 2) {

      int tdelay = strtol(flag, NULL, 10);

      if (tdelay > 0) {

        flags[5] = tdelay;
      
      } else {
        printErrorMessage(2, execName);
        return 0;
      }

    } else {
      printErrorMessage(0, execName);
      return 0;
    }
  
  }

  // user and system on as default if not specified
  if (flags[0] == 0 && flags[1] == 0) {
    flags[0] = 1;
    flags[1] = 1;
  }

  return 1;

}

// helper to print the help page
void printHelpPage(char *execName) {

  char *HELP_COMMANDS[] = {
    "--help (show this help page)",
    "--user (default argument, report users connected and sessions)",
    "--system (default argument, report system usage)",
    "--graphics (include graphical output where possible)",
    "--sequential (information output sequentially, no refreshing of screen)",
    "--samples=N (take N samples over the specified time)",
    "--tdelay=T (take N samples previously over T time in seconds)"
  };

  // iterate through array and print each message
  for (int i = 0; i < 7; i++) {
    printf("%s %s\n", execName, HELP_COMMANDS[i]);
  }

}


void printErrorMessage(int index, char *execName) {

  const char *ERROR_MESSAGES[] = {
    "Invalid command line arguments. Use '%s --help' to see a list of commands.\n",
    "Invalid command line arguments. Your flag '--samples=N' is invalid. N must be a positive integer. Use '%s --help' to see a list of commands.\n",
    "Invalid command line arguments. Your flag '--tdelay=T' is invalid. T must be a positive integer. Use '%s --help' to see a list of commands.\n",
  };

  printf(ERROR_MESSAGES[index], execName);

}

