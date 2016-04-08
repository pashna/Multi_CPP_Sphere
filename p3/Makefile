CC = g++
CFLAGS = -std=c++11 -Wall -Wextra -Wshadow -Wconversion -O2
LDFLAGS = -lboost_system

all : proxy

proxy : main.o
	$(CC) main.o -o proxy $(LDFLAGS)

main.o: main.cpp
	$(CC) $(CFLAGS) -c main.cpp

clean :
	rm -f proxy *.o
