
CXXFLAGS += -std=c++11

cc = g++
exe=server


server: server.cpp
	$(cc) -o server server.cpp

client: client.cpp
	$(cc) -o client client.cpp
	