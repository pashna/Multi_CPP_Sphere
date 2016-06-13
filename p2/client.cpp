//Linux
#include <iostream>
#include <unistd.h>
#include "client.hpp"
#include <sys/socket.h>
#include <sys/types.h>
#include <stdexcept>
#include <system_error>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <algorithm>
#define PORT 3100
#define SERVER_HOST "127.0.0.1"

void setnonblocking(int sockfd)
{
   if (fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFD, 0)|O_NONBLOCK) < 0)
        Client::throw_system_error(__LINE__, errno);
}

Client::Client() 
{
   // setnonblocking(0);
    pfd[0].fd = 0;
    pfd[0].events =  POLLIN;
    int sock = socket(PF_INET, SOCK_STREAM, 0);
    if(sock < 0)
        throw_system_error(__LINE__, errno);
    //setnonblocking(sock);
    pfd[1].fd = sock;
    pfd[1].events = POLLIN;
}

Client::~Client() 
{
    close(pfd[1].fd);
}

void Client::run() 
{
    sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = inet_addr(SERVER_HOST);
    if(connect(pfd[1].fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        throw_system_error(__LINE__, errno);
    setnonblocking(pfd[1].fd);
    int res;
    std::string buf;
    std::vector<char> msg;
    while(1) {
        res = poll(pfd, 2, -1);
        if(pfd[0].revents == POLLIN) {
            if(!std::getline(std::cin, buf))
                break;
            std::cout << "\x1b[1A\x1b[2K";
            buf.append("\n");
            send(buf);
        }
        if(pfd[1].revents == POLLIN) {
            recv(msg);
            while(fetch(msg));
        }
    }

}

void Client::throw_system_error(int line,int err) 
{
    std::cout << line << std::endl;
    throw std::system_error(err, std::system_category());
}

void Client::send(const std::string& buf) 
{
    int n = 0;
    while(n < buf.size()) {
        int len = std::min(1024, (int)buf.size() - n);
        int res = ::send(pfd[1].fd, buf.data() + n, len, 0);
        if(res < 0)
            throw_system_error(__LINE__ ,errno);
        n += res;
    }
}

void Client::recv(std::vector<char>& buf) 
{
    int size;
    do {
        int size = buf.size();
        buf.resize(buf.size() + 1024);
        int len = ::recv(pfd[1].fd, buf.data(), 1024, 0);
        if(len == 0)
            throw_system_error(__LINE__, errno);
        if(len < 0) {
            buf.resize(size);
            break;
        }
        buf.resize(size + (size_t)len);   
    }while(1);    
}

bool Client::fetch(std::vector<char>& buf) 
{
    std::string msg;
    auto it = std::find(buf.begin(), buf.end(), '\n');
    if(it == buf.end())
        return false;
    msg.assign(buf.begin(), it + 1);
    buf.erase(buf.begin(), it + 1);
    std::cout.write(msg.data(), msg.size());
    return true;
}

int main() 
{
    try {
        Client client;
        client.run();
    }
    catch (const std::exception& e) {
        std::cout << e.what() << std::endl;
    }
    return 0;
}
