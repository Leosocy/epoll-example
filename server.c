#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>

#define exit_if(r, ...)                                                    \
  if (r) {                                                                 \
    printf(__VA_ARGS__);                                                   \
    printf("\n");                                                          \
    printf("%s:%d [errno]: %d\t[errmsg]: %s\n", __FILE__, __LINE__, errno, \
           strerror(errno));                                               \
    exit(1);                                                               \
  }

int init_socket_listener(unsigned short port, int cap) {
  int listenfd = socket(AF_INET, SOCK_STREAM, 0);
  exit_if(listenfd < 0, "init socket failed");

  struct sockaddr_in serv_addr;
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(port);
  int ret = bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(struct sockaddr));
  exit_if(ret, "bind to 0.0.0.0:%d failed", port);

  ret = listen(listenfd, cap);
  exit_if(ret, "listen socket failed");

  return listenfd;
}

void set_nonblock(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  exit_if(flags < 0, "fcntl failed");
  int ret = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  exit_if(ret < 0, "fcntl failed");
}

int main() {
  unsigned short port = 6250;
  int socket_conn_queue_cap = 100;
  int listenfd = init_socket_listener(port, socket_conn_queue_cap);
  set_nonblock(listenfd);
  return 0;
}
