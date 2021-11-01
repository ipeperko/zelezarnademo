#ifndef HTTPSESSION_H
#define HTTPSESSION_H

#include "Object.h"
#include "HttpHandleRequest.h"
#include "WebsocketSession.h"

class HttpSession : public Object, public std::enable_shared_from_this<HttpSession>
{
    // This queue is used for HTTP pipelining.
    class queue
    {
        enum
        {
            // Maximum number of responses we will queue
            limit = 8
        };

        // The type-erased, saved work item
        struct work
        {
            virtual ~work() = default;
            virtual void operator()() = 0;
        };

        HttpSession& self_;
        std::vector<std::unique_ptr<work>> items_;

    public:
        explicit queue(HttpSession& self)
            : self_(self)
        {
            static_assert(limit > 0, "queue limit must be positive");
            items_.reserve(limit);
        }

        // Returns `true` if we have reached the queue limit
        bool is_full() const
        {
            return items_.size() >= limit;
        }

        // Called when a message finishes sending
        // Returns `true` if the caller should initiate a read
        bool on_write()
        {
            BOOST_ASSERT(!items_.empty());
            auto const was_full = is_full();
            items_.erase(items_.begin());
            if(! items_.empty())
                (*items_.front())();
            return was_full;
        }

        // Called by the HTTP handler to send a response.
        template<bool isRequest, class Body, class Fields>
        void operator()(http::message<isRequest, Body, Fields>&& msg)
        {
            // This holds a work item
            struct work_impl : work
            {
                HttpSession& self_;
                http::message<isRequest, Body, Fields> msg_;

                work_impl(
                        HttpSession& self,
                        http::message<isRequest, Body, Fields>&& msg)
                    : self_(self)
                    , msg_(std::move(msg))
                {
                }

                void operator()()
                {
                    http::async_write(
                        self_.stream_,
                        msg_,
                        beast::bind_front_handler(
                            &HttpSession::on_write,
                            self_.shared_from_this(),
                            msg_.need_eof()));
                }
            };

            // Allocate and store the work
            items_.push_back(boost::make_unique<work_impl>(self_, std::move(msg)));

            // If there was no previous work, start this one
            if(items_.size() == 1)
                (*items_.front())();
        }
    };

    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    std::shared_ptr<std::string const> doc_root_;
    queue queue_;

    boost::optional<HttpHandleRequest> parser_;

public:
    // Take ownership of the socket
    HttpSession(
        tcp::socket&& socket,
        std::shared_ptr<std::string const> doc_root)
        : Object("HttpSession")
        , stream_(std::move(socket))
        , doc_root_(std::move(doc_root))
        , queue_(*this)
    {
    }

    // Start the session
    void run()
    {
        do_read();
    }

private:
    void do_read()
    {
        // Construct a new parser for each message
        parser_.emplace();

        // Apply a reasonable limit to the allowed size
        // of the body in bytes to prevent abuse.
        //parser_->body_limit(10000);

        parser_->body_limit(std::numeric_limits<std::uint64_t>::max());

        // Set the timeout.
        stream_.expires_after(std::chrono::seconds(30));

        // Read a request using the parser-oriented interface
        http::async_read(
            stream_,
            buffer_,
            *parser_,
            beast::bind_front_handler(
                &HttpSession::on_read,
                shared_from_this()));
    }

    void on_read(beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        // This means they closed the connection
        if(ec == http::error::end_of_stream) {
            do_close();
            return;
        }

        if(ec) {
            log(error) << "read : " << ec.message();
            return;
        }

        // Parse ip address
        std::string ip_addr = stream_.socket().remote_endpoint().address().to_string();
        if (parser_->get().find("X-Real-IP") != parser_->get().end()) {
            ip_addr = parser_->get()["X-Real-IP"].to_string();
        }

        // See if it is a WebSocket Upgrade
        if(websocket::is_upgrade(parser_->get()))
        {
            // Create a websocket session, transferring ownership
            // of both the socket and the HTTP request.
            auto ws = std::make_shared<WebsocketSession>(std::move(stream_.release_socket()));
            ws->run(std::move(parser_->release()), ip_addr);
            return;
        }

        // Send the response
        HttpHandleRequest&& hr = std::move(*parser_);
        hr.setIpAddress(std::move(ip_addr));
        hr.handle_request(queue_);

        // If we aren't at the queue limit, try to pipeline another request
        if(! queue_.is_full())
            do_read();
    }

    void on_write(bool close, beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if(close)
        {
            // This means we should close the connection, usually because
            // the response indicated the "Connection: close" semantic.
            do_close();
            return;
        }

        if(ec) {
            log(error) << "write : " << ec.message();
            return;
        }

        // Inform the queue that a write completed
        if(queue_.on_write())
        {
            // Read another request
            do_read();
        }
    }

    void do_close()
    {
        // Send a TCP shutdown
        beast::error_code ec;
        stream_.socket().shutdown(tcp::socket::shutdown_send, ec);

        // At this point the connection is closed gracefully
    }
};

#endif // HTTPSESSION_H
