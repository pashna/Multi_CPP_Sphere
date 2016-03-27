// Архитектура - Linux

#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <vector>
#include <algorithm>

#define MAX_EVENTS 100
#define MAX_LENGTH 1025
#define PORT 3100
#define WELCOME "Welcome!\n"
#define LOG_IN "accepted connection\n"
#define LOG_OUT "connection terminated\n"
#define LOG(MSG) std::cout << (MSG) << std::flush
#define FAIL 1

int socket_and_bind(void) {
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (fd == -1) {
        std::cerr << strerror(errno) << std::endl;
        return FAIL;
    }

    int socketopt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &socketopt, sizeof(socketopt));

    sockaddr_in sock_addr;
    bzero(&sock_addr, sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_port = htons(PORT);
    sock_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int res = bind(fd, (sockaddr *)&sock_addr, sizeof(sock_addr));

    if (res == -1) {
        std::cerr << strerror(errno) << std::endl;
        return FAIL;
    }
    return fd;
}

int set_nonblock(int fd) {
    int flags;
    #if defined(O_NONBLOCK)
        if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
            flags = 0;
        return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    #else
        flags = 1;
        return ioctl(fd, FIONBIO, &flags);
    #endif
}

void remove_cfd(std::vector<int> &cfds, int fd) {
    cfds.erase(std::find(cfds.begin(), cfds.end(), fd));
    LOG(LOG_OUT);
    shutdown(fd, SHUT_RDWR);
    close(fd);
}

int main() {
    std::setlocale(LC_ALL, "Russian");
    std::vector<int> cfds;
    int msfd = socket_and_bind();

    if (msfd == FAIL) {
        return FAIL;
    }

    set_nonblock(msfd);

    int efd = epoll_create1(0);

    epoll_event event;
    event.data.fd = msfd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(efd, EPOLL_CTL_ADD, msfd, &event);

    epoll_event *events;
    events = (epoll_event *) calloc(MAX_EVENTS, sizeof event);

    int res = listen(msfd, SOMAXCONN);

    if (res == -1) {
        std::cout << strerror(errno) << std::endl;
        return FAIL;
    }

    while (true) {
        int N = epoll_wait(efd, events, MAX_EVENTS, -1);
        for (size_t i = 0; i < N; ++i) {
            if ((events[i].events & EPOLLIN) || !(events[i].events & EPOLLERR) && !(events[i].events & EPOLLHUP)) {
                if (msfd == events[i].data.fd) {
                    while (true) {
                        int ssfd = accept(msfd, 0, 0);
                        if (ssfd == -1)
                            break;
                        set_nonblock(ssfd);
                        event.data.fd = ssfd;
                        event.events = EPOLLIN | EPOLLET;
                        epoll_ctl(efd, EPOLL_CTL_ADD, ssfd, &event);
                        cfds.push_back(ssfd);
                        LOG(LOG_IN);
                        send(ssfd, WELCOME, sizeof WELCOME, 0);
                    }
                }
                else {
                    while (true) {
                        char buf[MAX_LENGTH] = {0};
                        buf[MAX_LENGTH - 1] = '\n';
                        ssize_t n = recv(events[i].data.fd, buf, sizeof(buf) - 1, 0);
                        if (n <= 0) {
                            break;
                        }
                        LOG(buf);
                        for (auto it = cfds.begin(), end = cfds.end(); it != end; ++it) {
                            send(*it, buf, n + 1, 0);
                        }

                    }
                }
            }
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP)) {
                remove_cfd(cfds, events[i].data.fd);
            }
        }
    }
    free(events);
    shutdown(msfd, SHUT_RDWR);
    close(msfd);
}