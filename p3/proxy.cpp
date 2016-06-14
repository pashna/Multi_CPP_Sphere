//Linux
#include "proxy.hpp"
#include <functional>
#include <fstream>
#include <string>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <memory>

buffer::buffer() :
    begin_(0),
    size_(0)
{}

void buffer::complete_recv(size_t size) 
{
    size_ += size;
}

void buffer::complete_send(size_t size)
{
    begin_ += size;
    size_ -= size;
    if(begin_ >= BUFFER_SIZE)
        begin_ -= BUFFER_SIZE;
}

std::array<boost::asio::const_buffer, 2> buffer::prepare_send() const
{
    std::array<ba::const_buffer, 2> result;
    size_t size = BUFFER_SIZE - begin_;
    if(size_ < size)
    {
        result[0] = ba::const_buffer(buf + begin_, size_);
        result[1] = ba::const_buffer(0, 0);
    }
    else 
    {
        result[0] = ba::const_buffer(buf + begin_, size);
        result[1] = ba::const_buffer(buf, size_ - size);
    }
    return result;
}

std::array<boost::asio::mutable_buffer, 2> buffer::prepare_recv()
{
    std::array<ba::mutable_buffer, 2> result;
    size_t size = begin_ + size_;
    if(size > BUFFER_SIZE)
    {
        result[0] = ba::mutable_buffer(buf + size - BUFFER_SIZE, BUFFER_SIZE - size_);
        result[1] = ba::mutable_buffer(0, 0); 
    }
    else 
    {
        result[0] = ba::mutable_buffer(buf + size, BUFFER_SIZE - size);
        result[1] = ba::mutable_buffer(buf, begin_);     
    }
    return result;
}

size_t buffer::size() const 
{
    return size_;
}

bridge::bridge(ba::io_service& io) :
    sock_one_(io),
    sock_two_(io),
    send_in_progress_one(false),
    send_in_progress_two(false),
    recv_in_progress_one(false),
    recv_in_progress_two(false),
    shutdown_one(false),
    shutdown_two(false)
{}

void bridge::start() 
{
    if(sock_one_.is_open() && sock_two_.is_open()) {
        try_recv_from_one();
        try_recv_from_two();
    }
}

void bridge::close() 
{
    bs::error_code err;
    sock_one_.close(err);
    sock_two_.close(err);
}

void bridge::on_recv_one(const bs::error_code& ec, size_t size) 
{
    recv_in_progress_one = false;
    buf_one_.complete_recv(size);
    if(!ec)
    {
        try_recv_from_one();
        try_send_to_two();
    }
    else if(ec.value() == ba::error::eof) {
        shutdown_one = true;
        try_send_to_two();
    }
    else
        close();
}
void bridge::on_recv_two(const bs::error_code& ec, size_t size) 
{
    recv_in_progress_two = false;
    buf_two_.complete_recv(size);
    if(!ec)
    {
        try_recv_from_two();
        try_send_to_one();
    }
    else if(ec.value() == ba::error::eof) {
        shutdown_two = true;
        try_send_to_one();
    }
    else
        close();
}

void bridge::on_send_one(const bs::error_code& ec, size_t size) 
{
    send_in_progress_one = false;
    buf_two_.complete_send(size);
    if(!ec)
    {
        try_recv_from_two();
        try_send_to_one();
    }
    else if(ec.value() != ba::error::eof)
        close();
}

void bridge::on_send_two(const bs::error_code& ec, size_t size) 
{
    send_in_progress_two = false;
    buf_one_.complete_send(size);
    if(!ec)
    {
        try_recv_from_one();
        try_send_to_two();
    }
    else if(ec.value() != ba::error::eof)
        close();
}

void bridge::try_recv_from_one() 
{
    if(recv_in_progress_one)
        return;
    if(buf_one_.size() != buffer::BUFFER_SIZE) {
        sock_one_.async_read_some(buf_one_.prepare_recv(),
                                  std::bind(&bridge::on_recv_one, shared_from_this(),
                                            std::placeholders::_1, 
                                            std::placeholders::_2));
        recv_in_progress_one = true;
    }
}

void bridge::try_recv_from_two() 
{
    if(recv_in_progress_two)
        return;
    if(buf_two_.size() != buffer::BUFFER_SIZE) {
        sock_two_.async_read_some(buf_two_.prepare_recv(),
                                  std::bind(&bridge::on_recv_two, shared_from_this(),
                                            std::placeholders::_1, 
                                            std::placeholders::_2));
        recv_in_progress_two = true;
    }
}

void bridge::try_send_to_one()
{
    if(send_in_progress_one)
        return;
    if(buf_two_.size()) {
        sock_one_.async_write_some(buf_two_.prepare_send(),
                                  std::bind(&bridge::on_send_one, shared_from_this(),
                                            std::placeholders::_1, 
                                            std::placeholders::_2));
        send_in_progress_one = true;
    }
    else if(shutdown_two) { 
        bs::error_code err;
        sock_one_.shutdown(ba::ip::tcp::socket::shutdown_send, err);
    }
}

void bridge::try_send_to_two() 
{
    if(send_in_progress_two)
        return;
    if(buf_one_.size()) {
        sock_two_.async_write_some(buf_one_.prepare_send(),
                                  std::bind(&bridge::on_send_two, shared_from_this(),
                                            std::placeholders::_1, 
                                            std::placeholders::_2));
        send_in_progress_two = true;
    }
    else if(shutdown_one) {
        bs::error_code err;
        sock_two_.shutdown(ba::ip::tcp::socket::shutdown_send, err);
    }
}

proxy::proxy(ba::io_service& io, const ba::ip::tcp::endpoint& src, const std::vector<ba::ip::tcp::endpoint>& dst) :
    acceptor(io, src, true),
    dst_(dst)
{
    do_accept();
}

void proxy::do_accept()
{
    std::shared_ptr<bridge> b = std::make_shared<bridge>(acceptor.get_io_service());    
    acceptor.async_accept(b->sock_one(), std::bind(&proxy::on_accept, this,
                                                   std::placeholders::_1, b));
}

void proxy::on_accept(const bs::error_code& ec, std::shared_ptr<bridge> b) 
{
    if(ec)
        throw bs::system_error(ec);
    
    b->sock_two().async_connect(dst_[(size_t)rand() % dst_.size()], 
                                std::bind(&proxy::on_connect, this, std::placeholders::_1, b));
    do_accept();
}

void proxy::on_connect(const bs::error_code& ec, std::shared_ptr<bridge> b) 
{
    if(!ec) {
       b->start(); 
    }
}

static void tokenize(const std::string& str,
              std::vector<std::string>& tokens,
              const std::string& delimiters = " ")
{
    std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
    std::string::size_type pos  = str.find_first_of(delimiters, lastPos);

    while (std::string::npos != pos || std::string::npos != lastPos)
    {
        tokens.push_back(str.substr(lastPos, pos - lastPos));
        lastPos = str.find_first_not_of(delimiters, pos);
        pos = str.find_first_of(delimiters, lastPos);
    }
}


int main(int argc, char** argv) {
    if(argc != 2)
        return -1;
    std::ifstream file(argv[1]);
    std::string line;
    ba::io_service io_service;
    std::vector<std::unique_ptr<proxy>> proxies;

    try {
        while(std::getline(file, line)) {
            std::cout << line << std::endl;
            std::vector<std::string> tokens;
            tokenize(line, tokens, ",: ");;
            int port = std::atoi(tokens.at(0).c_str());
            ba::ip::tcp::endpoint ep(ba::ip::tcp::v4(), port);
            std::vector<ba::ip::tcp::endpoint> d; 
            for(size_t i(1); i < tokens.size(); i += 2) {
                int x = std::atoi(tokens.at(i+1).c_str());
                ba::ip::address addr = ba::ip::address::from_string(tokens[i]); 
                d.push_back(ba::ip::tcp::endpoint(addr, x));
                std::cout << d.back() << std::endl;    
            }
            proxies.push_back(std::unique_ptr<proxy>(new proxy(io_service, ep, d)));
        }
        io_service.run();
    }
    catch(const bs::system_error& err) {
        std::cout << err.what() << std::endl;
        return -1;
    }
    catch(const std::out_of_range& err) {
        std::cout << "File configuration: Syntax Error!" << std::endl;
        return -1;
    }
    return 0;
}
