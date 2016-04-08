#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <iostream>
#include <fstream>
#include <cassert>

using namespace boost::asio;

struct client
{
    boost::shared_ptr <ip::tcp::socket> socket;
    boost::shared_ptr <streambuf> buffer;
    std::string message;
    boost::shared_ptr <ip::tcp::socket> partner;
    client() {}
};

static io_service service;
std::map <int, std::vector <std::pair <ip::address, int> > > config;

void terminate(client &c)
{
    std::cout << c.socket->remote_endpoint().address() << ":" << c.socket->remote_endpoint().port() << " destroyed" << std::endl;
    std::cout << c.partner->remote_endpoint().address() << ":" << c.partner->remote_endpoint().port() << " destroyed" << std::endl;

    boost::system::error_code now;
    c.socket->shutdown(ip::tcp::socket::shutdown_both, now);
    c.socket->close(now);
    c.partner->shutdown(ip::tcp::socket::shutdown_both, now);
    c.partner->close(now);
}

void on_write(client &c, const boost::system::error_code &error, size_t size)
{
    size = size;

    if (error) {
        terminate(c);
    }
}

void on_read(client &c, const boost::system::error_code &error, size_t size)
{
    size = size;
    std::istream is(c.buffer.get());
    is >> std::noskipws;
    char ch;

    while (is >> ch) {
        c.message += ch;
    }
    async_write(*c.partner, buffer(c.message), boost::bind(on_write, c, _1, _2));
    c.message = "";

    if (error) { // if EOF, then just stop reading, if another error, then terminate
        if (error.value() != error::eof) {
            terminate(c);
        }
    } else {
        async_read_until(*c.socket, *c.buffer, '\n', boost::bind(on_read, c, _1, _2));
    }
}

void start_accept(boost::shared_ptr <ip::tcp::acceptor> acceptor, boost::shared_ptr <ip::tcp::socket> socket);

void connect_handler(const boost::system::error_code &error)
{
    if (error) {
        return;
    }
}

void handle_accept(boost::shared_ptr <ip::tcp::acceptor> acceptor, boost::shared_ptr <ip::tcp::socket> socket, const boost::system::error_code &error)
{
    if (error) {
        std::cout << "Connection failed" << std::endl;
        return;
    }
    client nowclient;
    nowclient.socket = socket;
    nowclient.buffer = boost::shared_ptr <streambuf>(new streambuf);

    int src_port = acceptor->local_endpoint().port();
    std::pair <ip::address, int> addr = config[src_port][rand() % config[src_port].size()];
    nowclient.partner = boost::shared_ptr <ip::tcp::socket>(new ip::tcp::socket(service));

    boost::system::error_code connect_error;
    nowclient.partner->async_connect(ip::tcp::endpoint(addr.first, (unsigned short) addr.second), std::bind(connect_handler, connect_error));

    if (connect_error) {
        std::cout << "Connection failed" << std::endl;
        return;
    }

    client nowserver;
    nowserver.socket = nowclient.partner;
    nowserver.buffer = boost::shared_ptr <streambuf>(new streambuf);
    nowserver.partner = socket;

    std::cout << nowclient.socket->remote_endpoint().address() << ":" << nowclient.socket->remote_endpoint().port() << " connected to ";
    std::cout << nowserver.socket->remote_endpoint().address() << ":" << nowserver.socket->remote_endpoint().port() << std::endl;

    async_read_until(*nowclient.socket, *nowclient.buffer, '\n', boost::bind(on_read, nowclient, _1, _2));
    async_read_until(*nowserver.socket, *nowserver.buffer, '\n', boost::bind(on_read, nowserver, _1, _2));

    boost::shared_ptr <ip::tcp::socket> newsocket(new ip::tcp::socket(service));
    start_accept(acceptor, newsocket);
}

void start_accept(boost::shared_ptr <ip::tcp::acceptor> acceptor, boost::shared_ptr <ip::tcp::socket> socket)
{
    acceptor->async_accept(*socket, boost::bind(handle_accept, acceptor, socket, _1));
}

void parse(const char *path)
{
    std::fstream fin(path, std::fstream::in);
    std::string line;
    int src_port = -1;

    while (std::getline(fin, line)) {
        line += ',';
        int now = -1;
        while (line.find(',', now + 1) != std::string::npos) {
            std::string s = line.substr(now + 1, line.find(',', now + 1));
            now = int(line.find(',', now + 1));

            if (s.find(':') == std::string::npos) {
                src_port = atoi(s.c_str());
            } else {
                int pos = int(s.find_first_not_of(" "));
                std::string addr = s.substr(pos, s.find_first_of(" :", pos) - pos);
                int port = atoi(s.substr(s.find_first_not_of(" ", s.rfind(':') + 1)).c_str());
                config[src_port].push_back(std::make_pair(ip::address::from_string(addr), port));
            }
        }
    }
}

int main(int argc, char **argv)
{
    if (argc == 2)
        parse(argv[1]);
    else {
        std::cout << "Config-File Please!" << std::endl;
        return 1;
    }


    for (auto elem: config) {
        int port = elem.first;
        boost::shared_ptr <ip::tcp::acceptor> acceptor(new ip::tcp::acceptor(service, ip::tcp::endpoint(ip::tcp::v4(), (unsigned short) port)));
        boost::shared_ptr <ip::tcp::socket> socket(new ip::tcp::socket(service));
        start_accept(acceptor, socket);
    }

    service.run();
    return 0;
}
