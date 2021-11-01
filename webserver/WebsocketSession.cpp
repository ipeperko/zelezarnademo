#include "WebsocketSession.h"
#include "WebsocketDataBus.h"
#include "Application.h"

WebsocketSession::WebsocketSession(tcp::socket&& socket)
    : Object("WebsocketSession")
    , ws_(std::move(socket))
{
}

WebsocketSession::~WebsocketSession()
{
    WebsocketDataBus::instance().unregisterSession(this);
}

void WebsocketSession::on_send(std::shared_ptr<const std::string> const& ss)
{
    log(trace) << "websocket session " << this << " - on_send thread id " << std::this_thread::get_id();

    // Lock mutex while pushing to the queue
    std::unique_lock lock(mtx_queue_);

    // Always add to queue
    queue_.push(ss);

    // Are we already writing?
    if (queue_.size() > 1)
        return;

    // We don't need mutex locked here any more
    lock.unlock();

    tp_send_begin_ = std::chrono::steady_clock::now();

    // We are not currently writing, so send this immediately
    ws_.async_write(
        net::buffer(*queue_.front()),
        beast::bind_front_handler(
            &WebsocketSession::on_write,
            shared_from_this()));
}

void WebsocketSession::on_write(beast::error_code ec, std::size_t n)
{
    log(trace) << "websocket session " << this << " - on_write thread id " << std::this_thread::get_id();

    // Handle the error, if any
    if (ec) {
        log(error) << "write : " << ec.message();
        return;
    }

    tp_send_end_ = std::chrono::steady_clock::now();
    double us = std::chrono::duration_cast<std::chrono::microseconds>(tp_send_end_ - tp_send_begin_).count();
    log(debug) << "Websocket send finished " << n << " bytes - elapsed time " << us / 1000 << " msec";

    // Lock mutex
    std::unique_lock lock(mtx_queue_);

    // Remove the string from the queue
    queue_.pop();

    // Send the next message if any
    if (!queue_.empty()) {

        // we can release mutex as no one could remove the first element from queue
        lock.unlock();

        tp_send_begin_ = std::chrono::steady_clock::now();

        // Write next message
        ws_.async_write(
            net::buffer(*queue_.front()),
            beast::bind_front_handler(
                &WebsocketSession::on_write,
                shared_from_this()));
    }
}

void WebsocketSession::on_accept(beast::error_code ec)
{
    // Handle the error, if any
    if (ec) {
        log(error) << "accept : " << ec.message();
        return;
    }

    // Add this session to the list of active sessions
    WebsocketDataBus::instance().registerSession(this);

    // Send status message to webclient
    std::thread([self = shared_from_this()]() {
        self->send(std::make_shared<std::string const>(Application::instance().statusMessage()));
    }).detach();

    // Read a message
    ws_.async_read(
        buffer_,
        beast::bind_front_handler(
            &WebsocketSession::on_read,
            shared_from_this()));
}

void WebsocketSession::on_read(beast::error_code ec, std::size_t)
{
    // This indicates that the websocket_session was closed
    if (ec == websocket::error::closed) {
        log(info) << "Websocket closed session " << this;
        return;
    }

    if (ec) {
        log(error) << "read : " << ec.message();
        return;
    }

    try {
        std::string data = boost::beast::buffers_to_string(buffer_.data());
        log(debug) << "received " << data;
        Application::instance().acceptMessage(std::move(data));
    }
    catch (std::exception& e) {
        log(error) << "websocket error : " << e.what();
    }

    // Clear the buffer
    buffer_.consume(buffer_.size());

    // Read another message
    ws_.async_read(
        buffer_,
        beast::bind_front_handler(
            &WebsocketSession::on_read,
            shared_from_this()));
}

void WebsocketSession::send(std::shared_ptr<std::string const> ss)
{
    {
        auto lg = log(trace);
        lg << "Websocket sending " << ss->size() << " bytes : " << ss->substr(0, 200);
        if (ss->size() > 200) {
            lg << " ...";
        }
    }

    // Post our work to the strand, this ensures
    // that the members of `this` will not be
    // accessed concurrently.
    net::post(
        ws_.get_executor(),
        beast::bind_front_handler(
            &WebsocketSession::on_send,
            shared_from_this(),
            ss));
}

void WebsocketSession::disconnect()
{
    ws_.async_close({}, [this](std::error_code code) {
        log(info) << "... ws close ..." << code;
    });
}
