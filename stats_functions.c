#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <utmp.h>
#include <sys/sysinfo.h>
#include <string.h>
#include <math.h>
#include "stats_functions.h"

#define MAX_STRING_LEN 4096

#define RAM_GRAPHICS_SCALE 0.1
#define CPU_GRAPHICS_SCALE 2.0

void getUserUsage(char[MAX_STRING_LEN]);
void getMemoryUsage(char[MAX_STRING_LEN], int graphics, int sampleSize, int sampleCount, char history[sampleSize][256], double historyRam[sampleSize]);
void getCPUUsage(char string[MAX_STRING_LEN], int graphics, int* lastTotalTime, int* lastIdleTime, int sampleSize, int sampleCount, char history[sampleSize][256]);
double getUsagePercent(int totalTime, int idleTime);
void getCPUTimes(int *totalTime, int *idleTime);
int getNumCPUCores();

void handleReportUsers(int *flags, int pipes[2]) {

  int samples = flags[4];
  int tdelay = flags[5]; 

  for (int i = 0; i < samples; i++) {

    char string[MAX_STRING_LEN] = "";

    getUserUsage(string);

    write(pipes[1], string, MAX_STRING_LEN);

    sleep(tdelay);

  }

  endutent();

}

void handleReportMemory(int *flags, int pipes[2]) {

  int graphics = flags[2];
  int samples = flags[4];
  int tdelay = flags[5];

  char memoryStringHistory[samples][256];
  double memoryUsageHistory[samples];

  for (int i = 0; i < samples; i++) {

    char string[MAX_STRING_LEN] = "";

    getMemoryUsage(string, graphics, samples, i + 1, memoryStringHistory, memoryUsageHistory);

    write(pipes[1], string, MAX_STRING_LEN);
    sleep(tdelay);
  }

}

void handleReportCPU(int *flags, int pipes[2]) {

  int graphics = flags[2];
  int samples = flags[4];
  int tdelay = flags[5];

  int totalTime;
  int idleTime;

  for (int i = 0; i < samples; i++) {

    char string[MAX_STRING_LEN] = "";

    char cpuStringHistory[samples][256];

    if (i == 0) {
      // grab baseline
      getCPUTimes(&totalTime, &idleTime);

      strcpy(string, "----------CPU-Usage-------------------\n");
 
      char cpuCores[64];
      snprintf(cpuCores, sizeof(cpuCores), "Number of CPU Cores: %d\n", getNumCPUCores());
      strncat(string, cpuCores, (MAX_STRING_LEN - strlen(string) - 1) * sizeof(char));

      char* end = "Grabbing baseline sample for usage next sample...\n--------------------------------------\n"; 
      strncat(string, end, (MAX_STRING_LEN - strlen(string) - 1) * sizeof(char));

    } else {
      getCPUUsage(string, graphics, &totalTime, &idleTime, samples, i + 1, cpuStringHistory);
    }

    write(pipes[1], string, MAX_STRING_LEN);

    sleep(tdelay);
  }

}

void getUserUsage(char string[MAX_STRING_LEN]) {

  strcpy(string, "----------Users-----------------------\n");

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

    char entry[512];
    snprintf(entry, sizeof(entry), "%s    %s (%s) \n", user, device, host);

    strncat(string, entry, (MAX_STRING_LEN - strlen(string) - 1) * sizeof(char));
       
    // format and print the user

    userEntry = getutent();
  }

  char* end = "--------------------------------------\n"; 

  strncat(string, end, (MAX_STRING_LEN - strlen(string) - 1) * sizeof(char));

}


void getMemoryUsage(char string[MAX_STRING_LEN], int graphics, int sampleSize, int sampleCount, char history[sampleSize][256], double historyRam[sampleSize]) {

  strcpy(string, "----------Memory-Usage----------------\n");

  // create the buffer
  struct sysinfo memory;

  if (sysinfo(&memory) == -1) {
    perror("Error Fetching Memory Usage... sysinfo\n");
    return;
  }

  // get and process the values from sysinfo into formats we like.

  double toGB = 1000000000.0;

  double totalRam = ((double) memory.totalram) / toGB;
  double totalVirtualRam = totalRam + ((double) memory.totalswap) / toGB;
  
  double usedRam = totalRam - ((double) memory.freeram) / toGB; 
  double usedVirtualRam = totalVirtualRam - ((double) (memory.freeram + memory.freeswap)) / toGB;

  snprintf(history[sampleCount - 1], sizeof(history[sampleCount - 1]), "Physical: %.2f GB / %.2f GB       Virtual: %.2f GB / %.2f GB       ", usedRam, totalRam, usedVirtualRam, totalVirtualRam);

  if (graphics == 1) {

    int maxGraphicsLength = (int) (totalRam / RAM_GRAPHICS_SCALE) + 5;

    char graphicsLine[maxGraphicsLength];
    historyRam[sampleCount - 1] = usedRam;

    // initialize the string
    strcpy(graphicsLine, "|");

    // check if first entry
    if (sampleCount == 1) {
      strcat(graphicsLine, "*");
    } else {

      double lastEntry = historyRam[sampleCount - 2];
      double ramDelta = usedRam - lastEntry;

      double deltaAbsolute = fabs(ramDelta);

      for (double i = 0.0; i < deltaAbsolute; i += RAM_GRAPHICS_SCALE) {

        // concatenate characters based on increase/decrease
        if (ramDelta < 0) {
          strcat(graphicsLine, ":");
        } else {
          strcat(graphicsLine, "#");
        }

      }

      if(ramDelta < 0) {
        strcat(graphicsLine, "@");
      } else {
        strcat(graphicsLine, "*");
      }

      char delta[10];
      
      snprintf(delta, sizeof(delta), " %.2f", ramDelta);

      strncat(graphicsLine, delta, (maxGraphicsLength - strlen(graphicsLine) - 1) * sizeof(char));

    }

    strncat(history[sampleCount - 1], graphicsLine, 256);
  }

  for (int i = 0; i < sampleCount; i++) {
    strncat(string, history[i], (MAX_STRING_LEN - strlen(string) - 2) * sizeof(char));
    strncat(string, "\n", (MAX_STRING_LEN - strlen(string) - 1) * sizeof(char));
  }

  char* end = "--------------------------------------\n"; 

  strncat(string, end, (MAX_STRING_LEN - strlen(string) - 1) * sizeof(char));

}

void getCPUUsage(char string[MAX_STRING_LEN], int graphics, int* lastTotalTime, int* lastIdleTime, int sampleSize, int sampleCount, char history[sampleSize][256]) {

  strcpy(string, "----------CPU-Usage-------------------\n");

  char cpuCores[64];
  snprintf(cpuCores, sizeof(cpuCores), "Number of CPU Cores: %d\n", getNumCPUCores());
  strncat(string, cpuCores, (MAX_STRING_LEN - strlen(string) - 1) * sizeof(char));
 
  int totalTime;
  int idleTime;

  getCPUTimes(&totalTime, &idleTime);

  int totalTimeDelta = totalTime - *lastTotalTime;
  int idleTimeDelta = idleTime - *lastIdleTime;

  double usagePercent = getUsagePercent(totalTimeDelta, idleTimeDelta);

  *lastTotalTime = totalTime;
  *lastIdleTime = idleTime;

  char cpuUsageString[256];
  snprintf(cpuUsageString, sizeof(cpuUsageString), "CPU Usage: %.2f%%\n", usagePercent);
  strncat(string, cpuUsageString, (MAX_STRING_LEN - strlen(string - 1)) * sizeof(char));

  if (graphics == 1) {

    int maxGraphicsLength = (int) (100 / CPU_GRAPHICS_SCALE) + 5;

    char graphicsLine[maxGraphicsLength];

    // initialize string
    strcpy(graphicsLine, "|");

    for (double i = 0.0; i < usagePercent; i += CPU_GRAPHICS_SCALE) {
      // for every unit of scale, concatenate a | character
      strcat(graphicsLine, "|");
    }

    char usageString[10];

    snprintf(usageString, sizeof(usageString), " %.2f%%", usagePercent);

    strncat(graphicsLine, usageString, (maxGraphicsLength - strlen(graphicsLine) - 1) * sizeof(char));

    strncpy(history[sampleCount - 1], graphicsLine, sizeof(history[sampleCount - 1]));

    for (int i = 1; i < sampleCount; i++) {
      strncat(string, history[i], (MAX_STRING_LEN - strlen(string) - 2) * sizeof(char));
      strncat(string, "\n", (MAX_STRING_LEN - strlen(string) - 1) * sizeof(char));
    }

  }

  char* end = "--------------------------------------\n"; 

  strncat(string, end, (MAX_STRING_LEN - strlen(string) - 1) * sizeof(char));

}

int getNumCPUCores() {

  int count = 0;

  FILE *cpuinfo = fopen("/proc/cpuinfo", "r");

  if (cpuinfo == NULL) {
    perror("Error fetching cpu info... /proc/cpuinfo cannot be opened");
    return -1;
  }

  char currentLine[10]; // 9 for processor and 1 for \0

  while (fgets(currentLine, 10, cpuinfo) != NULL) {
    if (strcmp(currentLine, "processor") == 0) {
      count++;
    }
  }

  fclose(cpuinfo);

  return count;

}

void getCPUTimes(int *totalTime, int *idleTime) {
 
  FILE *stat = fopen("/proc/stat", "r");

  if (stat == NULL) {
    perror("Error Fetching CPU Usage... /proc/stat cannot be opened");
    return;
  }

  int currentTotalTime = 0;
  int currentIdleTime = 0;

  char cpu[10];

  fscanf(stat, "%s", cpu);

  // make sure file is formatted how we want

  if (strcmp(cpu, "cpu") != 0) {
    perror("Error Fetching CPU Usage... /proc/stat formatted incorrectly");
    return;
  }

  for (int i = 0; i < 7; i++) {

    int time;

    // proceed to get the next 7 columns
    fscanf(stat, "%d", &time);

    currentTotalTime += time;

    if (i == 3) {
      // idle is the 4th column
      currentIdleTime = time;
    }

  }

  *totalTime = currentTotalTime;
  *idleTime = currentIdleTime;

  fclose(stat);

}

double getUsagePercent(int totalTime, int idleTime) {
  // turn the total time and idle time into a usage percent
  // note, this is equivalent to the equation given in the a3 handout and is taken directly from my a1
  return (1.0 - ((double) idleTime) / ((double) totalTime)) * 100.0; 
}

