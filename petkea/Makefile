CC = g++
CFLAGS = -Wall -g


pet: pet-kea.o
	$(CC) $(CFLAGS) -o pet-out pet-kea.o

output: main.o process.o pet-kea.o
	$(CC) $(CFLAGS) -o output main.o process.o pet-kea.o
main.o: main.cpp process.h
	$(CC) $(CFLAGS) -c  main.cpp 

process.o: process.cpp process.h
	$(CC) $(CFLAGS) -c  process.cpp process.h
pet-kea.o: pet-kea.cpp pet-kea.hpp
	$(CC) $(CFLAGS) -c pet-kea.cpp pet-kea.hpp

clean:
	rm *.o *.gch output *.dat

dat:
	rm *.dat