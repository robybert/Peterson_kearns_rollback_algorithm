CC = g++
CFLAGS = -Wall -g



output: main.o process.o
	$(CC) $(CFLAGS) -o output main.o process.o
main.o: main.cpp process.h
	$(CC) $(CFLAGS) -c  main.cpp process.h

process.o: process.cpp process.h
	$(CC) $(CFLAGS) -c  process.cpp process.h


clean:
	rm *.o output