CC = g++
CFLAGS = -Wall -g



output: main.o process.o
	$(CC) $(CFLAGS) -o output main.o
main.o: main.cpp process.cpp
	$(CC) $(CFLAGS) -c process.cpp main.cpp

process.o: process.cpp
	$(CC) $(CFLAGS) -c process.cpp


clean:
	rm *.o output