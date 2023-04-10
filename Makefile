CC=gcc
LIBS=-lm
ARGS=-Wall
RM=rm
OBJFILES=main.o stats_functions.o

sysinfo: $(OBJFILES) 
	$(CC) $^ $(ARGS) $(LIBS) -o $@ 

main.o: main.c stats_functions.h process_info.h
	$(CC) -c $< $(ARGS) $(LIBS) -o $@

stats_functions.o: stats_functions.c stats_functions.h
	$(CC) -c $< $(ARGS) $(LIBS) -o $@

.PHONY: clean
clean:
	$(RM) $(OBJFILES)

