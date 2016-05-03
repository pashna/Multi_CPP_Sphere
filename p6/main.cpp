#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <iostream>
#include <cassert>

using namespace boost::asio;

enum
{
    KEY_SIZE = 32,
    VAL_SIZE = 256,
    CACHE_SIZE = 4096
};

int hash(const char *s)
{
    int n = (int) strlen(s);
    unsigned long long res = 0;

    for (int i = 0; i < n; i++) {
        res = res * 179 + s[i];
    }
    return (int) (res % CACHE_SIZE);
}

struct elem
{
    char key[KEY_SIZE];
    char val[VAL_SIZE];
    int tll;
    bool occ;
};

static elem *hashset;

bool set(const char *key, const char *val, int tll)
{
    int start = hash(key);
    int pos = start;
    
    do {
        if (!hashset[pos].occ) {
            break;
        }
        pos++;
    } while (pos != start);

    if (hashset[pos].occ) {
        return false;
    }
    memcpy(hashset[pos].key, key, strlen(key) + 1);
    memcpy(hashset[pos].val, val, strlen(val) + 1);
    hashset[pos].occ = true;
    hashset[pos].tll = tll;
    return true;
}

bool get(const char *key, char *val)
{
    int start = hash(key);
    int pos = start;

    do {
        if (!hashset[pos].occ) {
            break;
        }

        if (strcmp(key, hashset[pos].key) == 0) {
            break;
        }
        pos++;
    } while (pos != start);
    
    if (!hashset[pos].occ || strcmp(key, hashset[pos].key) != 0) {
        return false;
    }
    memcpy(val, hashset[pos].val, strlen(hashset[pos].val) + 1);
    return true;
}

struct client
{
    boost::shared_ptr <ip::tcp::socket> socket;
    boost::shared_ptr <streambuf> buffer;
    client() {}
};

static std::vector <client> clients;
static io_service service;

void on_write(client &c, const boost::system::error_code &error, size_t size)
{
    size = size;
    c = c;

    if (error) {
        return;
    }
    return;
}

void on_read(client &c, const boost::system::error_code &error, size_t size)
{
    size = size;
    std::istream is(c.buffer.get());
    std::string cmd, key, val, result;
    int tll;

    is >> cmd;

    if (cmd == "get") {
        is >> key;
        getline(is, val);

        if (key.size() >= KEY_SIZE) {
            result = "error key is too big\n";
        } else {
            char res[VAL_SIZE];
            bool ret = get(key.data(), res);

            if (ret) {
                result = "ok " + key + ' ' + std::string(res) + '\n';
            } else {
                result = "error no such element\n";
            }
        }
    } else if (cmd == "set") {
        is >> tll >> key;
        getline(is, val);
        int ptr = 0;

        while (ptr < int(val.size()) && isspace(val[ptr])) {
            ptr++;
        }
        val = val.substr(ptr);

        if (key.size() >= KEY_SIZE) {
            result = "error key is too big\n";
        } else if (val.size() >= VAL_SIZE) {
            result = "error val is too big\n";
        } else {
            bool ret = set(key.data(), val.data(), tll);

            if (ret) {
                result = "ok " + key + ' ' + val + '\n';
            } else {
                result = "error no enough memory\n";
            }
        }
    } else {
        getline(is, val);
        result = "error no such operation\n";
    }

    async_write(*c.socket, buffer(result), boost::bind(on_write, c, _1, _2));

    if (error) {
        boost::system::error_code now;
        c.socket->shutdown(ip::tcp::socket::shutdown_both, now);
        c.socket->close(now);
        std::cout << "connection terminated" << std::endl;
        return;
    }

    async_read_until(*c.socket, *c.buffer, "\n", boost::bind(on_read, c, _1, _2));
}

void start_accept(boost::shared_ptr <ip::tcp::acceptor> acceptor, boost::shared_ptr <ip::tcp::socket> socket);

void handle_accept(boost::shared_ptr <ip::tcp::acceptor> acceptor, boost::shared_ptr <ip::tcp::socket> socket, const boost::system::error_code &error)
{
    if (error) {
        return;
    }
    std::cout << "accepted connection" << std::endl;
    client now;
    now.socket = socket;
    now.buffer = boost::shared_ptr <streambuf> (new streambuf);
    clients.push_back(now);

    async_read_until(*now.socket, *now.buffer, "\n", boost::bind(on_read, now, _1, _2));

    boost::shared_ptr <ip::tcp::socket> newsocket(new ip::tcp::socket(service));
    start_accept(acceptor, newsocket);
}

void start_accept(boost::shared_ptr <ip::tcp::acceptor> acceptor, boost::shared_ptr <ip::tcp::socket> socket)
{
    acceptor->async_accept(*socket, boost::bind(handle_accept, acceptor, socket, _1));
}

int main(void)
{
    hashset = (elem *) calloc(CACHE_SIZE, sizeof(*hashset));

    boost::shared_ptr <ip::tcp::acceptor> acceptor(new ip::tcp::acceptor(service, ip::tcp::endpoint(ip::tcp::v4(), 3100)));
    boost::shared_ptr <ip::tcp::socket> socket(new ip::tcp::socket(service));

    start_accept(acceptor, socket);
    service.run();
}
