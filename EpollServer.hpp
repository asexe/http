#ifndef EPOLL_SERVER_HPP
#define EPOLL_SERVER_HPP

#include "ThreadPool.hpp"
#include <sys/epoll.h>
#include <string>


class EpollServer {
private:
    int server_fd;
    int epoll_fd;
    std::string directory;
    ThreadPool pool;

    void eventLoop();

public:
    EpollServer(int server_fd, const std::string& directory, size_t thread_count);
    ~EpollServer();
    void start();
};

#endif // EPOLL_SERVER_HPP