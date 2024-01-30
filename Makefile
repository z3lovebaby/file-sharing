# Define variables
CXX = g++
CXXFLAGS = -g -Wall

# Define targets
all: server client

server: server.cpp utils.h utils.cpp commands.h commands.cpp
	$(CXX) $(CXXFLAGS) server.cpp utils.cpp commands.cpp -o server

client: client.cpp utils.h utils.cpp commands.h commands.cpp
	$(CXX) $(CXXFLAGS) client.cpp utils.cpp commands.cpp -o client

clean:
	rm -f server client
