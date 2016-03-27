#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/poll.h>
#include <vector>
#include <set>
#include <string.h>

#define PORT 3100
#define POLL_SIZE 2
#define FAIL -1
#define STDIN 0
#define BUF_SIZE 1025


int main() {
    int msfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (msfd == -1) {
        std::cerr << strerror(errno) << std::endl;
        return FAIL;
    }

//    int socketopt = 1;
//    setsockopt(msfd, SOL_SOCKET, SO_REUSEADDR, &socketopt, sizeof(socketopt));

    struct sockaddr_in sock_addr;
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_port = htons(PORT);
    sock_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (connect(msfd, (const sockaddr *) (const void*) &sock_addr, sizeof 	(sock_addr)) == -1) {
        std::cerr << strerror(errno) << std::endl;
        return FAIL;
    }

    struct pollfd set[POLL_SIZE];
    set[0].fd = msfd;
    set[0].events = POLLIN;

    set[1].fd = STDIN;
    set[1].events = POLLIN;

    size_t set_size = 2;
    std::string prev_str = "";

    while (true) {
        poll(set, set_size, -1);
        if (set[0].revents & POLLIN) { // msfd
            char buf[BUF_SIZE] = {0};
            buf[BUF_SIZE - 1] = '\0';
            int recv_size = (int) recv(msfd, buf, sizeof(buf) - 1, 0);
            if (recv_size > 0) {
                std::cout << buf << std::flush;
            }
        }
        if (set[1].revents & POLLIN) { // stdin
            std::string message;
            //std::getline(std::cin, message);
            //std::cout <<    message;
            //std::cin >> message;

            char a;

            int was_entered = 0;
            while(std::cin.get(a)) {
                //std::cout << "a=" << a << std::endl;
                if (a != '\n') {
                    was_entered = 0;
                    message += prev_str + a;
                    //std::cout << "message=" << message << std::endl;
                    prev_str = "";
                } else {
                    send(msfd, message.c_str(), message.size(), 0);
                    prev_str = "";

                }
            }
            //int was_entered = -1;
            /*
            for (int i=0; i<message.length(); i++) {
                if (message[i] == '\n') {
                    was_entered = i;
                    message = prev_str + message;
                    send(msfd, message.c_str(), message.size(), 0);
                    prev_str = "";
                }
            }
            if (was_entered < 0) {
                std::cout << "NO ENTER";
                prev_str = message;
            }
            */
            message += '\n';
            send (msfd, message.c_str(), message.size(), 0);
            

        }
    }    
    shutdown(msfd, SHUT_RDWR);
    close(msfd);
    return 0;
}
