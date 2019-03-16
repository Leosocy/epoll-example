#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <wait.h>
#include <unistd.h>

// --mode 0|1:
//   0: 测试`惊群`效应，在主进程创建epoll fd，创建socket，并bind(注意惊群效应在单核CPU不存在)。
//   1: 测试socket reuse port，不同进程监听同一个地址和端口。
// --et:
//   默认epoll为LT模式，传此参数将改为ET模式。
// --loop-accept:
//   是否循环accept socket，解决ET模式下丢失事件。
// --port xxxx:
//   socket监听端口。
// --sleep n:
//   epoll_wait和accept之间睡眠秒数，为了放大`惊群效应`的影响，不填默认不睡眠。

#define MODE_THUNDER_HERD 0
#define MODE_REUSE_PORT 1

int worker_num = 4;
int mode = 0;
int et = 0;
int loop_accept = 0;
int port = 0;
int slp = 0;

#define exit_if(r, ...)                                                                                             \
  if (r) {                                                                                                          \
    printf(__VA_ARGS__);                                                                                            \
    printf("\n[ERR at pid:%d %s:%d] [errno]:%d [errmsg]:%s", getpid(), __FILE__, __LINE__, errno, strerror(errno)); \
    fflush(stdout); \
    exit(1);                                                                                                        \
  }

#define log(fmt, ...) do {\
   printf("[LOG at pid:%d %s:%d] " #fmt "\n", getpid(), __FILE__, __LINE__, ##__VA_ARGS__);     \
   fflush(stdout); \
} while(0)

void set_socket_binding(int listen_fd, const char* serv_ip, unsigned short port, int backlog) {
  struct sockaddr_in serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(serv_ip);
  serv_addr.sin_port = htons(port);
  int ret = bind(listen_fd, (struct sockaddr*)&serv_addr, sizeof(struct sockaddr));
  exit_if(ret, "bind to %s:%d failed", serv_ip, port);

  ret = listen(listen_fd, backlog);
  exit_if(ret, "listen socket failed");
}

void set_socket_nonblock(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  exit_if(flags < 0, "fcntl failed");
  int ret = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  exit_if(ret < 0, "fcntl failed");
}

void set_socket_reuseport(int fd) {
  int val = 1;
  int ret = setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val));
  exit_if(ret < 0, "socket reuse port failed")
}

int init_socket_listener() {
  const char* serv_ip = "0.0.0.0";
  int backlog = 100;
  int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  exit_if(listen_fd < 0, "init socket failed");
  set_socket_nonblock(listen_fd);
  if (mode == MODE_REUSE_PORT) {
    set_socket_reuseport(listen_fd);
  }
  set_socket_binding(listen_fd, serv_ip, port, backlog);
  log("begin listening at %s:%d", serv_ip, port);
  return listen_fd;
}

void register_epoll_events(int epfd, int listen_fd, int events, int op) {
  struct epoll_event ev;
  memset(&ev, 0, sizeof(ev));
  ev.events = events;
  if (et) {
    ev.events |= EPOLLET;
  }
  ev.data.fd = listen_fd;
  int ret = epoll_ctl(epfd, op, listen_fd, &ev);
  exit_if(ret, "epoll_ctl failed");
}

void send_hello(int conn_fd) {
    char buf[] = "Hello from server.\n";
    write(conn_fd, buf, strlen(buf));
}

void handle_accept(int epfd, int listen_fd) {
  struct sockaddr_in client_addr;
  socklen_t addrlen = sizeof(client_addr);
  do {
  int conn_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &addrlen);
  if (conn_fd <= 0) {
    if (!et) {
      log("thunder herd");
    }
    return;
  }
  log("accept connection from %s:%d", inet_ntoa(client_addr.sin_addr), client_addr.sin_port);
  send_hello(conn_fd);
  close(conn_fd);
  } while (loop_accept);
}

void epoll_loop_once(int epfd, int listen_fd, int wait_ms) {
  const int max_events_count = 10;
  struct epoll_event ready_events[max_events_count];
  int nfds = epoll_wait(epfd, ready_events, max_events_count, wait_ms);
  if (nfds > 0 && slp) {
    sleep(slp);
  }
  int i = 0;
  for (i = 0; i < nfds; ++i) {
    int fd = ready_events[i].data.fd;
    int events = ready_events[i].events;
    if (events & (EPOLLIN |EPOLLERR )) {
      if (fd == listen_fd) {
        handle_accept(epfd, listen_fd);
      } else {
        log("RECV");
        // TODO: handle recv
      }
    } else if (events & EPOLLOUT) {
      // TODO: handle send
    } else {
      exit_if(1, "unknown event");
    }
  }
}

void start_worker(int epfd, int listen_fd) {
  if (mode == MODE_REUSE_PORT) {
    listen_fd = init_socket_listener();
    epfd = epoll_create(1);
    register_epoll_events(epfd, listen_fd, EPOLLIN, EPOLL_CTL_ADD);
  }

  int loop_wait_ms = 5000;
  while (1) {
    epoll_loop_once(epfd, listen_fd, loop_wait_ms);
  }

  // clean up some resources.
  close(listen_fd);
  close(epfd);
}

void parse_args(int argc, char** argv) {
  const char* usage = "Usage: ./server --mode 0|1 [--et] [--loop-accept] --port 1234 [--sleep n(s)]";
  exit_if(argc < 5, usage);
  int i = 1;
  while (i < argc) {
    if (strcmp("--mode", argv[i]) == 0) {
      mode = atoi(argv[++i]);
    } else if (strcmp("--et", argv[i]) == 0) {
      et = 1;
    } else if (strcmp("--loop-accept", argv[i]) == 0) {
      loop_accept = 1;
    } else if (strcmp("--port", argv[i]) == 0) {
      port = atoi(argv[++i]);
    } else if (strcmp("--sleep", argv[i]) == 0) {
      slp = atoi(argv[++i]);
    } else {
        exit_if(1, usage);
    }
    i++;
  }
}

int main(int argc, char** argv) {
  parse_args(argc, argv);
  int epfd = 0, listen_fd = 0;
  if (mode == MODE_THUNDER_HERD) {
    listen_fd = init_socket_listener();
    epfd = epoll_create(1);
    register_epoll_events(epfd, listen_fd, EPOLLIN, EPOLL_CTL_ADD);
  }
  int i = 0;
  for (i = 0; i < worker_num; ++i) {
    pid_t pid = fork();
    exit_if(pid < 0, "fork failed");
    if (pid == 0) {
      start_worker(epfd, listen_fd);
    }
  }

  while (wait(NULL) > 0)
    ;
  exit_if(errno == ECHILD, "wait child process failed")
  return 0;
}
