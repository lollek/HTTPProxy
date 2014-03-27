CXX = g++
CXXFLAGS = -Wall -Wextra -Werror -pedantic -std=c++11 -O3 -g -Weffc++
OBJS = TCPSocket/TCPSocket.o HTTPProxy.o main.o
TARGET = proxy

all: $(OBJS)
	$(CXX) $^ -o $(TARGET)

run: all
	./$(TARGET) 8080

clean:
	$(RM) $(OBJS) $(TARGET)
