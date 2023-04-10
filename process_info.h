#include <sys/types.h>
#include <stdbool.h>

typedef int ProcessType;

typedef struct processInfo {
  pid_t pid;
  int pipeAccess[2]; 
  ProcessType processType;
  bool success;
  bool isChild;
} ProcessInfo;


