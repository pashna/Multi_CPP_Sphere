CC = g++
CFLAGS = -std=c++11 -Wall -Wextra -Wshadow -Wconversion -O2

all : sort

sort : main.o
	$(CC) main.o -fopenmp -o sort

main.o : main.cpp
	$(CC) $(CFLAGS) -fopenmp -c main.cpp

clean :
	rm -f sort *.o
