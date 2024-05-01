CC = g++
CFLAGS = -O3 -g -std=c++20 


pet: pet-kea.o
	$(CC) $(CFLAGS) -o pet-out pet-kea.o

output: main.o process.o pet-kea.o
	$(CC) $(CFLAGS) -o output $^
main.o: main.cpp process.hpp
	$(CC) $(CFLAGS) -c  $^

process.o: process.cpp process.hpp
	$(CC) $(CFLAGS) -c  $^
pet-kea.o: pet-kea.cpp pet-kea.hpp
	$(CC) $(CFLAGS) -c $^
clean:
	rm *.o *.gch output *.dat

dat:
	rm *.dat
csv:
	rm *.csv