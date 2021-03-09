
CXXFLAGS += -std=c++11

cc = g++
exe=server


server: main.cpp
	$(cc) -o server main.cpp
