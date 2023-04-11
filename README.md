# System Information Tool v2 (Assignment 3) for Linux. Made for CSCB09

Note, many function implementations noted here are similar to A1.

Author: Lucas Ilea

# Table of contents

<!--ts-->

- [Documentation](#documentation)
  - [Compilation](#compilation)
  - [Arguments](#arguments)
  - [Code](#code)

<!--te-->

## Documentation

## Compilation

To compile to an executable, run

`$ make`

To clean the object files, run

`$ make clean`

## Arguments

Running the program with `$ ./sysinfo --help` you will see the possible arguments:

```bash
./sysinfo --help (show this help page)
./sysinfo --user (default argument, report users connected and sessions)
./sysinfo --system (default argument, report system usage)
./sysinfo --graphics (include graphical output where possible)
./sysinfo --sequential (information output sequentially, no refreshing of screen)
./sysinfo --samples=N (take N samples over the specified time)
./sysinfo --tdelay=T (take N samples previously over T time in seconds)
```

By default, running `$ ./sysinfo` will run the program with the user and system arguments, aka
`$ ./sysinfo --user --system`

To see users connected and their sessions, run
`$ ./sysinfo --user`

To see system information including CPU and memory utilization, run
`$ ./sysinfo --system`
Note: CPU utilization will be collecting a baseline sample during the first sample. This is specified when running it.

To see how memory information is being calculated and converted, see `getMemoryUsage()`.

To have a graphical output of the CPU and memory utilization, run
`$ ./sysinfo --graphics`  
Note: This will have no effect on user statistics.

---

###### Graphical Legend

':' represents a unit of relative decrease of the memory utilization since the last sample.
'#' represents a unit of relative increase of the memory utilization since the last sample.
A relative decrease graphical string will be suffixed by '@'.
A relative increase graphical string will be suffixed by '\*'.

---

To view the output of the program without the terminal refreshing after samples, run
`$ ./sysinfo --sequential`
Note: This is particularly useful when redirecting the output to a file.

To modify the amount of samples taken run,
`$ ./sysinfo --samples=N`
where N represents the amount of samples you want as a positive integer.

---

###### Example

`$ ./sysinfo --samples=5` will only take 5 samples.

---

To modify the time between samples, run,
`$ ./sysinfo --tdelay=N`
where N represents the time taken between samples in seconds as a positive integer.

---

###### Example

`$ ./sysinfo --tdelay=2` will take 2 seconds between samples.

---

By default, the program will take 10 samples with 1 second in between each sample aka
`$ ./sysinfo --samples=10 --tdelay=1`

---

The program will also take positional arguments, the first of which being sample size and the second being the time delay.  
`$ ./sysinfo 5 2`  
For example, the above arguments will print 5 samples in total with a delay of 2 seconds in between each one.

---

## Code

The structure of the project is as follows:

`main.c` handles the code to manage and read from the processes using pipes, as well as putting everything together.  
`stats_functions.c` handles the implementation to get memory, user, and cpu usage, format them, and write them to the pipes.  
`stats_functions.h` holds the function prototypes to be implemented by `stats_functions.c`  
`process_info.h` holds the typedef for a `ProcessType` which is just a unique integer for each type of process, ie. `memory (0), user (1), cpu (2)` and the typedef for a struct called
`ProcessInfo` that holds the pid of a process, its pipe fds, process type (using the typedef above), whether it was successful, and whether it is a child process returning this struct.

###### main, main.c

In the `main()` function, we define an array of 6 integers that represent the possible argument flags passed to the program. 0 represents off, and 1 represents on. For samples and time delay, we just put their respective default values, since they are always on.

Then we call a function `setFlags(int*, int, char**)` that will take in a reference to the flags array, argc value, and a reference to the argv array. It will take the arguments provided from the user, parse them, and update the flags array accordingly. If `setFlags()` returns 0, there was an error and we return 0 in main to terminate execution of the program.

If there is no error, then we call a function `handleProcesses(int*)` that will take in a reference to the flags array, and accordingly compose the proper output to the terminal based on the flags specified.

###### handleProcesses, main.c

In the `handleProcesses(int*)` function, we first initialize some variables.

Firstly is an array of 3 `ProcessInfo` structs. We initialize them with invalid structs which have their `success` field set to `false`.

###### handleReportUsers, stats_functions.c

In the `handleReportUsers(int*, int[2])` function, we simply loop through all the samples, set the user usage string using `getUserUsage()`, write it to the pipe argument using `pipes[1]`, and sleep for the specified time delay.

###### handleReportMemory, stats_functions.c

The `handleReportMemory(int*, int[2])` function has the exact same implementation as `handleReportUsers()`, except we use the `getMemoryUsage()` function and we initialize a string array and double array beforehand for the history implementation in `getMemoryUsage()`.

###### handleReportCPU, stats_functions.c

The `handleReportCPU(int*, int[2])` function has the same implementation as the above handler functions, but has a few more edge cases.

If it is the first sample, we grab baseline CPU times and set them to `totalTime` and `idleTime` variables respectively. We then format the output string to specify that this sample, we are grabbing baseline samples. During this sample, we still show the number of cpu cores.

For all other samples, we just use the `getCPUUsage()` function to write the information to the `string` variable.

###### getUserUsage, stats_functions.c

In the `getUserUsage()` function, we use `utmp.h` to get all the user entries.

Firstly, we reset the pointer to the beginning of the users using `setutent()`. Then we can declare a struct `struct utmp *userEntry = getutent();` which will store a process into the struct. Afterwards we loop until all the entries have been gone over. Inside the loop, we make sure that the process is a user process, then we get the user's information, print it out accordingly, then assign the next user using `getutent()`.

After the loop we call `endutent()` to properly close the stream.

###### getNumCPUCores, stats_functions.c

In the `getNumCPUCores()` function, we use `fopen()` to open the `/proc/cpuinfo` file, and use `fgets` to loop over lines and add to the count every time we encounter the `processor` key. Each time this is found corresponds to a core in the CPU.  
We then close the file and return the count.

###### displaySystemInformation, main.c

In the `displaySystemInformation()` function, we create a buffer struct where we use `uname()` to populate it with system information. If `uname()` returns -1, we have an error and we return out of the function. Otherwise, we can simply access the buffer and print out the system information.

###### getMemoryUsage, stats_functions.c

In the `getMemoryUsage(char[MAX_STRING_LEN], int, int sampleSize, int, char[sampleSize][256], double[sampleSize])` function, we create a buffer struct, then use `sysinfo()` to populate it with memory information. If `sysinfo()` returns -1, we have an error and we return out of the function. Otherwise, we calculate and convert the information into usable data as follows.

total_ram = total_bytes / 1000000000  
total_virtual_ram = total_ram + (total_swap / 1000000000)  
used_ram = total_ram - (free_ram / 1000000000)  
used_virtual_ram = total_virtual_ram - ((free_ram + free_swap) / 1000000000)

Since the memory utilization part shows previous samples, we must store the previously outputted strings in some history array. This is the `char history[sampleSize][256]` parameter. To save the formatted string in the array, we also have an `int sampleCount` parameter that indicates the sample that's currently being rendered. Using these values, we can use `snprintf(history[sampleCount - 1], sizeof(history[sampleCount - 1]), formatted_string_here, values...)` to save the string to the proper location in the array.

If graphics were specified, we will now create a new string called `graphicsLine` that we will concatenate using `strncat()` to our previous string at `history[sampleCount - 1]` after composing it.

To compose `graphicsLine`, we first check what our maximum length can be based on how much ram exists in the system, and what the scale we set is. For example, if `RAM_GRAPHICS_SCALE = 0.1`, then for every 0.1 gb change of the memory utilization, we will add a single graphical character. Similarily to saving the strings in history, we also need to save the memory used in a double array to calculate relative usage. We do this by setting `historyRam[sampleCount - 1] = usedRam;`

We then use `strcpy()` to copy a single '|' to the string. We then check if this is the first sample. If it is, then we will just set the baseline key as '\*'. Otherwise, we need to calculate the relative utilization.

To do this, we just get the last entry in the double array by using `historyRam[sampleCount - 2]`, and then find the delta with the current memory used by subtracting them. Afterwards, we can just loop until a double exceeds this delta, incrementing by the `RAM_GRAPHICS_SCALE` each time. However, if the delta is negative, our loop is incorrect. For purely the loop's functionality, we use the absolute value of the delta instead, given by `fabs` in `math.h`.

Inside of the loop we check whether the delta is negative or positive. If it is negative, we concatenate the character ':' and if it is positive, we concatenate the character '#'.

Similarily, outside of the loop we cap the string using the characters '@' and '\*' depending on whether the delta was negative or positive respectively.

Afterwards we also want to append the delta to the string, so we use `snprintf()` to format it to a string, then use `strncat()` to concatenate it to `graphicsLine`.

Finally, we just loop through the history by using `sampleCount` as the exclusive upper bound, and concatenate each respective line to the `string` argument using `strncat()`.

###### getCPUUsage, stats_functions.c

In the `getCPUUsage(char[MAX_STRING_LEN], int, int*, int*, int sampleSize, int, char[sampleSize][256])` function, we use `getNumCPUCores()` to find the number of CPU cores in the system, and concatenate that to `string`. Afterwards, we need to calculate the CPU utilization.

We declare two integer variables, `totalTime` and `idleTime`. We then pass their addresses to `getCPUTimes(int*, int*)` to populate them with the total time the CPU has been active for, and the CPU's idle time respectively.

Since getting these CPU times will give us the total time the CPU has been working since the system has started, and similarily for idle time, we must find the deltas for these values after some time. To do this, we subtract the `lastTotalTime` and `lastIdleTime` parameters from `totalTime` and `idleTime` respectively. We then call `getUsagePercent(int, int)` to find the CPU utilization percent in the form of a double. Afterwards, we can set `lastTotalTime` to `totalTime` and `lastIdleTime` to `idleTime` for the next sample.

However, we don't have a `lastTotalTime` and `lastIdleTime` for the first sample. To account for this, we will grab a baseline sample in `handleReportCPU(int*, int[2])` and only run this function after the 1st sample. This is further explained in `handleReportCPU(int*, int[2])`.

We then format the CPU usage.

If graphics were specified, we implement it similarly to `getMemoryUsage()`. The difference is that we don't need to account for a delta and append different characters. All we do is append the character `|` for every unit of scale specified by `CPU_GRAPHICS_SCALE`.

Finally, we just loop through the history by using `sampleCount - 1` as the exclusive upper bound, and concatenate each respective line to the `string` argument using `strncat()`.

###### getCPUTimes, stats_functions.c

In the `getCPUTimes(int*, int*)` function, we open and parse the `/proc/stat` file to gather CPU times.

To do this, we use `FILE *stat = fopen("/proc/stat", "r")`. If this file is NULL, then we've encountered an error and we return out of the function.

We then define two integer variables, `currentTotalTime` and `currentIdleTime` and set them to 0.

Next we define a char array called `cpu` and use `fscanf(stat, "%s', cpu)` to get the first string in the file and put it into `cpu`.

Afterwards, we compare this string using `strcmp(cpu, "cpu")` to make sure we are looking at the right line in the file. Otherwise, we return out of the function.

Now we can proceed to looping over the seven columns in the first line.

Inside the loop, we define an integer variable, `time`, then set it to the next integer in the file using `fscanf(stat, "%d", &time)`. Then we add it to `currentTotalTime`. If the column we're going over is the idle column (column 4), then we also set `currentIdleTime` to `time`.

After the loop, we then set the `totalTime` and `idleTime` parameters to `currentTotalTime` and `currentIdleTime`.

We then close the file using `fclose(stat)`.

###### getUsagePercent, stats_functions.c

In the `getUsagePercent(int, int)` function, we just return `(1 - (idleTime / totalTime)) * 100` to get the amount of time the CPU has not been idle in a percent.  
Note: This is equivalent to the expressions given in the assignment handout.

###### getCurrentProcessUsage, stats_functions.c

In the `getCurrentProcessUsage()` function, we open the `/proc/self/status` file using `FILE *status = fopen("/proc/self/status", "r")`.

We then define a char array of length 30 as `key`, and an integer variable called `currentValue`.

We then try to find the `VmRSS:` row in the file by looping through until the key is equivalent to that string, checking using `strcmp(key, "VmRSS:")`.

Inside of the loop, we use `fscanf(status, "%s %d", key, &currentValue)` to keep saving the new key and value of that key to their respective variables. We also check if it's the end of the file, in which case we close the file using `fclose(status)` and we return -1. Once we come across `VmRSS:`, the loop exits and we close the file using `fclose(status)`, then return `currentValue` since it has been assigned the correct value that is paired with `VmRSS:`.

###### refreshScreen, main.c

In the `refreshScreen()` function, we print two escape codes. Firstly we run `printf("\033[0;0H]")` to set the cursor to zero to make sure samples get printed starting in the top left corner of the terminal. Then we run `printf("\033[2J")` to clear the screen.

###### setFlags, main.c

In the `setFlags(int*, int, char**)` function, we first save the name of the executable ran into a variable by `argv[0]`. This is so that we can print the command name in the help page depending on what the name of the file is, in case the user changed it. Then we loop from 1 to the length of the arguments so that we can go through all the arguments provided to the program through the command line after the executable name.

Afterwards we define and declare a string, `flag`, using `strtok(argv[i], "=")` to get the key of the argument in case the argument is one where a variable option can be provided, such as `--samples=N and --tdelay=N`. If there is no such variable option, it will still set the flag to the key regardless, so we don't have to check for it. Then we check if the flag provided is `--help` using `strcmp()` in which case we call `printHelpPage(execName)` using the name of the executable as the argument. We then return 0 to halt the program as specified in `main()`.

Similarily, we compare the other flags using `strcmp()`. In the case that we come across `--user`, we simply set `flags[0]` to 1 to turn it on as that is the index that corresponds to users. The same goes for `--system`, `--graphics`, and `--sequential`.

If we find a `--samples`, we then need to get the value after the `=`. To do this, we use `strtok()` again on the same pointer. If it returns null, then we know the user didn't follow the required format, in which case we print the corresponding error message and return 0. Otherwise, we then convert the value they specified to an int named `sampleSize` using `strtol()`. We then check if `sampleSize` is valid (>0), and then we set the appropriate element in the `flags` array, `flags[4]` to it. If it is not valid, we then print the corresponding error message and return 0.

The same goes for `--tdelay` as above.

We also check if it is the first and second argument provided. If none of these match, we have positional arguments, and we parse them similarily as above and set them.

Then if an argument does not match any of these, the final else statement will send an error message and return 0.

Outside of the loop, we check if user and system in `flags` were set. If both were not set, then we set them on as default.

Finally we return 1 since if we got here, there has been no error.

###### printHelpPage, main.c

In the `printHelpPage(char*)` function, we simply just loop over all items in the help message array and print them.

###### printErrorMessage, main.c

In the `printErrorMessage(int, char*)` function, we simply print out the error message corresponding to the index argument.
