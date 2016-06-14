
#ifndef PROXY_HPP__
#define PROXY_HPP__
#define BOOST_ASIO_ENABLE_HANDLER_TRACKING
#include<boost/asio.hpp>
#include<vector>
#include<iostream>

namespace ba = boost::asio;
namespace bs = boost::system;

class buffer {
public:
    enum {BUFFER_SIZE = 4096};
    buffer();
    void complete_recv(size_t size);
    void complete_send(size_t size);
    std::array<boost::asio::const_buffer, 2> prepare_send() const;
    std::array<boost::asio::mutable_buffer, 2> prepare_recv();
    size_t size() const;
private:
    char buf[BUFFER_SIZE];
    size_t begin_;
    size_t size_;
};

class bridge : public std::enable_shared_from_this<bridge> {
public:
    bridge(ba::io_service& io);
    void start();
    ba::ip::tcp::socket & sock_one() { return sock_one_; }
    ba::ip::tcp::socket & sock_two() { return sock_two_; }
    void close();
private:
    void on_recv_one(const bs::error_code& ec, size_t size);
    void on_recv_two(const bs::error_code& ec, size_t size);
    void on_send_one(const bs::error_code& ec, size_t size);
    void on_send_two(const bs::error_code& ec, size_t size);
    void try_recv_from_one();
    void try_recv_from_two();
    void try_send_to_one();
    void try_send_to_two();
    ba::ip::tcp::socket sock_one_;
    ba::ip::tcp::socket sock_two_;
    buffer buf_one_;
    buffer buf_two_;
    bool send_in_progress_one;
    bool send_in_progress_two;
    bool recv_in_progress_one;
    bool recv_in_progress_two;
    bool shutdown_one;
    bool shutdown_two;
};

class proxy {
public:
    proxy(ba::io_service& io, const ba::ip::tcp::endpoint& src, const std::vector<ba::ip::tcp::endpoint>& dst);
    proxy(proxy&& obj) : acceptor(std::move(obj.acceptor)), dst_(std::move(obj.dst_)) {}
    void run();
private:
    void do_accept();
    void on_accept(const bs::error_code& ec, std::shared_ptr<bridge> b);
    void on_connect(const bs::error_code& ec, std::shared_ptr<bridge> b);
    ba::ip::tcp::acceptor acceptor;
    std::vector<ba::ip::tcp::endpoint> dst_;
};

#endif //PROXY_HPP__
