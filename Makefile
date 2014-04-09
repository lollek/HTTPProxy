CXX = g++
CXXFLAGS = -Wall -Wextra -Werror -pedantic -std=c++11 -O3 -g -Weffc++
OBJS = TCPSocket/TCPSocket.o HTTPProxy.o main.o
TARGET = proxy

TESTOBJS = TCPSocket/TCPSocket.o HTTPProxy.o tests/tests.o
TEST = tests/test

all: $(OBJS)
	$(CXX) $^ -o $(TARGET)

run: all
	./$(TARGET) 8080

test: $(TESTOBJS)
	$(CXX) -o $(TEST) -lgtest -lgtest_main $^
	$(TEST)

clean:
	$(RM) $(OBJS) $(TARGET) $(TEST) $(TESTOBJS)


