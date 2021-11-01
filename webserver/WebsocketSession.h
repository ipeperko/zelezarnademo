#ifndef WEBSOCKETSESSION_H
#define WEBSOCKETSESSION_H

#include "Object.h"
#include "Webserver_common.h"
#include <queue>
#include <thread>


class WebsocketSession : public Object, public std::enable_shared_from_this<WebsocketSession>
{
    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;

    std::mutex mtx_queue_;
    std::queue< std::shared_ptr<std::string const> > queue_;
    std::string ip_address_;
    std::chrono::steady_clock::time_point tp_send_begin_;
    std::chrono::steady_clock::time_point tp_send_end_;

    void on_send(std::shared_ptr<std::string const> const& ss);
    void on_write(beast::error_code ec, std::size_t);
    void on_accept(beast::error_code ec);
    void on_read(beast::error_code ec, std::size_t);

public:

    explicit WebsocketSession(tcp::socket&& socket);

    ~WebsocketSession() override;

    void send(std::shared_ptr<std::string const> ss);

    void disconnect();

    template<class Body, class Allocator>
    void run(http::request<Body, http::basic_fields<Allocator>> req, std::string ip_addr)
    {
        ip_address_ = std::move(ip_addr);

        // Set suggested timeout settings for the websocket
        ws_.set_option(
                websocket::stream_base::timeout::suggested(
                        beast::role_type::server));

        // Set a decorator to change the Server of the handshake
        ws_.set_option(websocket::stream_base::decorator(
                [](websocket::response_type& res)
                {
                    res.set(http::field::server, WebserverVersion::get());
                }));

        // Accept the websocket handshake
        ws_.async_accept(
                req,
                beast::bind_front_handler(
                        &WebsocketSession::on_accept,
                        shared_from_this()));
    }
};

#endif // WEBSOCKETSESSION_H
