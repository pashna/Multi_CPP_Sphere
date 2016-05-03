CC = g++
CFLAGS = -std=c++11 -Wall -Wextra -Wshadow -Wconversion -g

all : server

server : main.o
	$(CC) main.o -lboost_system -o server

main.o : main.cpp
	$(CC) $(CFLAGS) -c main.cpp

clean :
	rm -f server *.o
