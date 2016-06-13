//Linux
#include "server.hpp"
#include <iostream>
#include <sys/epoll.h>
#include <fcntl.h>
#include <system_error>
#include <arpa/inet.h>
#include <algorithm>
#define PORT 3100
#define COUNT_CLIENT 1000

//std::ifstream file;

void setnonblocking(int sockfd)
{
   fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0)|O_NONBLOCK);
}

Server::Server()  
{
    int fd = socket(PF_INET, SOCK_STREAM, 0);
    int yes = 1;
    if(fd < 0)
        throw_system_error(__LINE__, errno);
    setnonblocking(fd);
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    msock = Handle(fd);
    fd = epoll_create(COUNT_CLIENT);
    if(fd < 0)
        throw_system_error(__LINE__, errno);
    epfd = Handle(fd);
    epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.ptr = 0;
    if(epoll_ctl(fd, EPOLL_CTL_ADD, msock.get(), &ev) < 0)
        throw_system_error(__LINE__, errno);
}

Server::~Server() {}

void Server::run() 
{
    sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(3100);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if(bind(msock.get(), (sockaddr*) &addr, sizeof(addr)) < 0)
        throw_system_error(__LINE__, errno);
    if(listen(msock.get(), 20) < 0)
        throw_system_error(__LINE__, errno);
    
    int ev_cnt;    
    epoll_event events[COUNT_CLIENT];
    std::cout << "Server run..." << std::endl;
    while(1) {
        ev_cnt = epoll_wait(epfd.get(), events, COUNT_CLIENT, -1);
        if(ev_cnt < 0)
            throw_system_error(__LINE__, errno);
        for(int i(0); i != ev_cnt; ++i) {
            if(events[i].data.ptr == 0) 
                handle_server_events();
            else 
                handle_client_events(reinterpret_cast<Connection*>(events[i].data.ptr));
        }
    }         
}

void Server::handle_server_events() 
{
    sockaddr_in cl_addr = {0};
    std::string fmt;
    socklen_t socklen = sizeof(cl_addr);
    int csock;
    do {
        csock = accept(msock.get(), (sockaddr*) &cl_addr, &socklen);
        if (csock < 0) {
            if (errno != EAGAIN)
                throw_system_error(__LINE__, errno);
            else
                break;
        }
        setnonblocking(csock);
        Connection* conn = clients.create(Handle(csock), &cl_addr);
        epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;
        ev.data.ptr = conn;
        if(epoll_ctl(epfd.get(), EPOLL_CTL_ADD, csock, &ev) < 0)
            throw_system_error(__LINE__, errno);
        fmt.clear();
        fmt.append("Welcome: ");
        fmt.append(conn->ip());
        fmt.append("\n");
        conn->send(fmt);
        std::cout << "accepted connection " << conn->ip() << std::endl;
    }while(1);
}

void Server::handle_client_events(Connection* conn) 
{
    std::string msg;
    std::string fmt;
    bool alive = conn->recv();
    while(conn->fetch(msg)) {
        fmt.clear();
        fmt.append(conn->ip());
        fmt.append(": ");
        fmt.append(msg);
        std::cout << fmt << std::flush;
        for(Connection* c = clients.head(); c ; c = c->next()) 
            c->send(fmt);
    }
    if(!alive) { 
        std::cout << "connection terminated " << conn->ip() << std::endl; 
        clients.destroy(conn);
    }
}

void Server::throw_system_error(int line, int err) 
{
    std::cout << "line error: " << line << std::endl;
    throw std::system_error(err, std::system_category());
}

Server::Connection::Connection(Handle&& csock, const sockaddr_in* caddr) :
    csock_(std::forward<Handle&&>(csock)),
    next_(0),
    prev_(0)
{
    if(inet_ntop(AF_INET, &(caddr->sin_addr), ip_, INET_ADDRSTRLEN) == NULL)
        ip_[0] = 0;
}

bool Server::Connection::recv() 
{
    do{
        size_t len = buf_.size();
        buf_.resize(len + 1024);
        int res = ::recv(csock_.get(), buf_.data() + len, 1024, 0);
        if (res == 0)
            return false;
        if (res < 0) {
            buf_.resize(len);
            return errno == EAGAIN;
        }
        buf_.resize(len + (size_t)res);
    }while(1);
    return true;
}

bool Server::Connection::fetch(std::string& mes) 
{
    auto it = std::find(buf_.begin(), buf_.end(), '\n');
    if(it == buf_.end())
        return false;
    mes.assign(buf_.begin(), it + 1);
    buf_.erase(buf_.begin(), it + 1);
    return true;
}
 
bool Server::Connection::send(const std::string& message)
{
    int n = 0;
    int size = (int)message.size();
    while(size > n) {
        int len = std::min(1024, size - n);
        int sent = ::send(csock_.get(), message.data() + n, len, 0);
        if(sent <= 0)
           // throw_system_error(__LINE__, errno);
            return false;
        n += sent;
    }
    return true;
}

Server::ConnectionList::ConnectionList() :
    head_(0),
    tail_(0)
    {}

Server::ConnectionList::~ConnectionList()
{
    while(head_) {
        Connection* next = head_->next_;
        delete head_;
        head_ = next;
    }
}

Server::Connection* Server::ConnectionList::create(Handle&& csock, const sockaddr_in* caddr)
{
    Connection* conn = new Connection(std::forward<Handle&&>(csock), caddr);
    conn->prev_ = tail_;
    (tail_ ? tail_->next_ : head_) = conn;
    tail_ = conn;
    return conn;
}

void Server::ConnectionList::destroy(Server::Connection* conn)
{
    (conn->prev_ ? conn->prev_->next_ : head_) = conn->next_;
    (conn->next_ ? conn->next_->prev_ : tail_) = conn->prev_;
    delete conn;
}

int main() {
    try {
        Server server;
        server.run();
    } 
    catch(const std::system_error& err)
    {
        std::cout << "error " <<  err.what() << std::endl;
    }
    return 0;
}
