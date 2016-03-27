CXX = g++
CXXFLAGS = -std=c++0x

.DEFAULT_GOAL := all

chatsrv: server.cpp
	$(CXX) $(CXXFLAGS) -o chatsrv server.cpp

chatclient: client.cpp
	$(CXX) $(CXXFLAGS) -o chatclient client.cpp

all: chatsrv chatclient

clean:
	rm chatclient chatsrv
