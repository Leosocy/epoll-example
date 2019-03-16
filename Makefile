CC=gcc
CXX=g++

OBJS=server client


all: $(OBJS)
server: server.c
	gcc -Wall -Werror -ggdb -o server server.c
fmt:
	clang-format -i *.c
clean:
	rm -rf $(OBJS)

