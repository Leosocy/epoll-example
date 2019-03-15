# 学习epoll源码后写此项目加深印象

## 关于几点学习总结

1. `epoll`不需要每次`wait`都将文件描述符从用户态拷贝到内核态，因为它在调用`epoll_ctl`时已经将对应的fd拷贝到了内核空间，并不存在网上说的一些什么内核空间和用户空间通过`mmap`共享一块内存的机制(具体实现可以看内核源码[这一行](https://code.woboq.org/linux/linux/fs/eventpoll.c.html#2012))。通过`epoll_create`创建的`eventpoll`对象和通过`epoll_ctl`创建的`epitem`对象都是存储在内核的`slab`中(至于slab是什么并没有深入研究，简单理解就是一块连续的读写效率都很高的内存)。
1. `epoll_ctl`会在对应fd的等待队列entry上注入`ep_poll_callback`函数，实现当fd就绪后唤醒epoll进程等待队列上的进程。
1. `epoll`只会将**就绪**的文件描述符对应的`epoll_event`从内核空间拷贝到用户空间(具体可以看[这一行](https://code.woboq.org/linux/linux/fs/eventpoll.c.html#1665))，相比于`select`每次都拷贝全部fd效率无疑大大提升。
1. ET模式和LT模式区别仅在于源码[这一段](https://code.woboq.org/linux/linux/fs/eventpoll.c.html#1677)，如果是边缘触发，那么就不加回可用列表，因此只能等到下一个可用事件触发的时候才会将对应的epi放到可用列表里面。简单来说，如果我写了一段socket程序，设置成LT模式，那么在收到监听socket可读事件时，可以仅`accept`一次，然后调用`epoll_wait`处理下一个链接；但是设置成ET模式就必须循环accept，因为在大并发的情况下，同时向服务器发起多个链接是很正常的，而监听socket只会触发一次可读事件，如果只接受一个链接那么会后面的链接就无法建立。


## example写了点啥

就是一个最简易的基于socket的C/S数据传输模型

## How to run

First, run `make all`

In terminal1

```shell
./server
```

In terminal2

```shell
./client
```

在client输入"xxx"，server会收到对应消息，然后回复"hello, xxx"

