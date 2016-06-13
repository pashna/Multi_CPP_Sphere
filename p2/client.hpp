
#ifndef __CLIENT__
#define __CLIENT__

//#include "handle.hpp"
#include <sys/poll.h>
#include <string>
#include <vector>

class Client {
public:
    Client();
    ~Client();
    void run();
    static void throw_system_error(int line, int err);
private:
    void send(const std::string& buf);
    void recv(std::vector<char>& mes);
    bool fetch(std::vector<char>& mes);
    pollfd pfd[2];
};

#endif //__CLIENT__
