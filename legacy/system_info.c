#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <utmp.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <math.h>

// constants to make argument handling more organized


const int STAT_COLUMNS = 7;
const int IDLE_COLUMN = 4;


// functionality
void displayUserUsage();
void displaySystemInformation();
void displayMemoryUsage(int, int sampleSize, int, char[sampleSize][256], double[sampleSize]);
void displayCPUUsage(int, int*, int*, int sampleSize, int, char[sampleSize][256]);
void getCPUTimes(int*, int*);
double getUsagePercent(int, int);
int getCurrentProcessUsage();
void composeStats(int*);
void refreshScreen();
void displayHeaderInfo(int, int, int);


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

    composeStats(flags);

  return 0;
}

// functions below here are for functionality

void displayUserUsage() {

  printf("----------Users-----------------------\n");

  // reset pointer
  setutent();

  struct utmp *userEntry = getutent();

  // loop until we're over all the users
  while (userEntry != NULL) {

    // make sure its a user 
    if ((userEntry -> ut_type) != USER_PROCESS) {
      userEntry = getutent();
      continue;
    }

    char *user = userEntry -> ut_user;
    char *device = userEntry -> ut_line;
    char *host = userEntry -> ut_host;

    // if host is blank, assume it is local
    if (strcmp(host, "") == 0) {
      strcpy(host, "local");
    }

    // format and print the user
    printf("%s    %s (%s) \n", user, device, host);

    userEntry = getutent();
  }

  printf("--------------------------------------\n");

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




int getCurrentProcessUsage() {

  FILE *status = fopen("/proc/self/status", "r");

  char key[30];
  int currentValue;

  // fscanf through the file until key is VmRSS, meaning we can then get
  // the value for the utilization of this program
  while (strcmp(key, "VmRSS:") != 0) {
    if (fscanf(status, "%s %d", key, &currentValue) == EOF) {
      fclose(status);
      return -1;
    }
  }

  fclose(status);

  return currentValue;

}

void displayHeaderInfo(int samples, int sampleNumber, int timeDelay) {

  printf("\n+-------------------------------------+\n\n");

  printf("%d samples every %d second(s)\n", samples, timeDelay);
  printf("Sample #%d\n\n", sampleNumber);

  printf("Memory Usage: %d kB\n\n", getCurrentProcessUsage());

}

// for routing flags to their respective functions

void composeStats(int *flags) {

  int user = flags[0];
  int system = flags[1];
  int graphics = flags[2];
  int sequential = flags[3];
  int samples = flags[4];
  int timeDelay = flags[5];

  double cpuSleepTime = 500000;
  
  // grab a baseline sample so that we can take delta of cpu times to
  // make it more accurate to realtime usage
  if (system == 1) {
    refreshScreen();
    printf("Grabbing Baseline Sample...\n");
    getCPUTimes(&totalTime, &idleTime);
    usleep(cpuSleepTime);
  }

  // initialize some arrays for redrawing history
  char memoryStringHistory[samples][256];
  double memoryUsageHistory[samples];


  for (int i = 1; i < samples + 1; i++) {

    if (sequential == 0) {
      // only refresh if sequential is not on
      refreshScreen();
    }

    displayHeaderInfo(samples, i, timeDelay);

    if (user == 1) {
      displayUserUsage();
    } 

    if (system == 1) {
      displaySystemInformation();
      displayMemoryUsage(graphics, samples, i, memoryStringHistory, memoryUsageHistory);
      displayCPUUsage(graphics, &totalTime, &idleTime, samples, i, cpuStringHistory);
    }

    printf("\n+-------------------------------------+\n");

    fflush(stdout); // flush stdout so that every interval it is shown
    sleep(timeDelay);
  }

  // properly end utent
  endutent();

}

// functions below here are for argument handling

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
        printf(ERROR_MESSAGES[1], execName);
        return 0;
      }

      // convert str to int
      int sampleSize = strtol(flag, NULL, 10);

      // ensure value is valid
      if (sampleSize > 0) {
      
        flags[4] = sampleSize;
      
      } else {
        printf(ERROR_MESSAGES[1], execName);
        return 0;
      }

    } else if (strcmp(flag, "--tdelay") == 0) {

      // similarly as above

      flag = strtok(NULL, "=");

      if (flag == NULL) {
        printf(ERROR_MESSAGES[2], execName);
        return 0;
      }

      int timeDelay = strtol(flag, NULL, 10);

      if (timeDelay > 0) {

        flags[5] = timeDelay;

      } else {
        printf(ERROR_MESSAGES[2], execName);
        return 0;
      }

    } else {
      printf(ERROR_MESSAGES[0], execName);
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

  int len = sizeof HELP_COMMANDS / sizeof HELP_COMMANDS[0];

  // iterate through array and print each message
  for (int i = 0; i < len; i++) {
    printf("%s %s\n", execName, HELP_COMMANDS[i]);
  }

}

