CC=gcc
CXX=g++

OBJS=server client


all: $(OBJS)
server: server.c
	gcc -Wall -Werror -o server server.c
client: client.c
	gcc -Wall -Werror -o client client.c
fmt:
	clang-format -i *.c
clean:
	rm -rf $(OBJS)

