
#ifndef __SERVER__
#define __SERVER__

#include<sys/socket.h>
#include<sys/types.h>
#include<stdexcept>
#include <netinet/in.h>
#include <string>
#include <unistd.h>
#include <sys/epoll.h>
#include <list>
#include <vector>

class Handle {
public:
    Handle(int fd) : fd_(fd) {}
    Handle() : fd_(-1) {}
    ~Handle() { if(fd_ >= 0) close(fd_); }  
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    Handle(Handle&& obj) : fd_(obj.fd_) { obj.fd_ = -1; }
    Handle& operator=(Handle&& obj) { 
        Handle temp(std::forward<Handle&&>(obj));
        temp.swap(*this);
        return *this;
    } 
    int get() const { return fd_; }
    void swap(Handle& obj) { std::swap(obj.fd_, fd_); }
    operator void*() { return reinterpret_cast<void*>(fd_ >= 0 ? 1 : 0 ); }
private:
    int fd_;
};

class Server {
public:
    Server();
    ~Server();
    void run();
    Server(const Server& obj) = delete;
    Server& operator=(const Server& obj) = delete;
private:
    static void throw_system_error(int line, int err);
   
    class ConnectionList;

    class Connection {
    public:
        Connection(Handle&& csock, const sockaddr_in* caddr);
        bool recv();
        bool fetch(std::string& mes);
        bool send(const std::string& message);
        const char* ip() const { return ip_; } 
        Connection* next() { return next_; }
        Connection* prev() { return prev_; }
        int get_sock() { return csock_.get(); }
    private:
        friend class ConnectionList;
        std::vector<char> buf_;
        char ip_[INET_ADDRSTRLEN];
        Handle csock_;
        Connection* next_;
        Connection* prev_;
    };
    
    class ConnectionList {
    public:
        ConnectionList();
        ~ConnectionList();
        Connection* create(Handle&& csock, const sockaddr_in* caddr);
        void destroy(Connection* connection);
        Connection* head() { return head_; }
        Connection* tail() { return tail_; }
    private:
        Connection* head_;
        Connection* tail_;
    };

    void handle_server_events();
    void handle_client_events(Connection* conn);
        
    Handle msock;
    Handle epfd;
    ConnectionList clients;
};

#endif //__SERVER__

