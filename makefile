CC = g++
CFLAGS = -std=c++17

all: program

program: main.cpp
	$(CC) $(CFLAGS) -o program main.cpp

run:
	./program

clean:
	rm -f program
