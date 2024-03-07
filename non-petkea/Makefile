CC = g++
CFLAGS = -Wall -g

output: main.o process.o
	$(CC) $(CFLAGS) -o output main.o process.o
main.o: main.cpp process.hpp
	$(CC) $(CFLAGS) -c  main.cpp process.hpp

process.o: process.cpp process.hpp
	$(CC) $(CFLAGS) -c  process.cpp process.hpp

clean:
	rm *.o *.gch output