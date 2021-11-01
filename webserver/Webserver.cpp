#include "Webserver.h"
#include "Webserver_common.h"
#include "HttpSession.h"
#include <boost/asio/signal_set.hpp>
#include <boost/asio/strand.hpp>
#include <thread>
#include <vector>


// Accepts incoming connections and launches the sessions
class Listener : public Object, public std::enable_shared_from_this<Listener>
{
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    std::shared_ptr<std::string const> doc_root_;

public:
    Listener(
        net::io_context& ioc,
        tcp::endpoint endpoint,
        std::shared_ptr<std::string const> doc_root)
        : Object("Listener")
        , ioc_(ioc)
        , acceptor_(net::make_strand(ioc))
        , doc_root_(std::move(doc_root))
    {
        beast::error_code ec;

        // Open the acceptor
        acceptor_.open(endpoint.protocol(), ec);
        if(ec)
        {
            log(error) << "open : " << ec.message();
            return;
        }

        // Allow address reuse
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if(ec)
        {
            log(error) << "set_option : " << ec.message();
            return;
        }

        // Bind to the server address
        acceptor_.bind(endpoint, ec);
        if(ec)
        {
            log(error) << "bind : " << ec.message();
            return;
        }

        // Start listening for connections
        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if(ec)
        {
            log(error) << "listen : " << ec.message();
            return;
        }
    }

    // Start accepting incoming connections
    void run()
    {
        do_accept();
    }

private:
    void do_accept()
    {
        // The new connection gets its own strand
        acceptor_.async_accept(
            net::make_strand(ioc_),
            beast::bind_front_handler(
                &Listener::on_accept,
                shared_from_this()));
    }

    void on_accept(beast::error_code ec, tcp::socket socket)
    {
        if(ec)
        {
            log(error) << "accept : " << ec.message();
        }
        else
        {
            // Create the http session and run it
            std::make_shared<HttpSession>(
                std::move(socket),
                doc_root_)->run();
        }

        // Accept another connection
        do_accept();
    }
};

int Webserver::run(unsigned short port) {

    auto const address = boost::asio::ip::make_address("0.0.0.0");
    log(info) << "Starting webserver port " << port;

    int threads = 10;

    // The io_context is required for all I/O
    boost::asio::io_context ioc{threads};

#   ifdef HTTP_SERVER_ASYNC_FLEX
    // The SSL context is required, and holds certificates
    ssl::context ctx{ssl::context::sslv23};
    load_root_certificates(ctx);
    // This holds the self-signed certificate used by the server
    load_server_certificate(ctx);
#   endif

    // Create and launch a listening port
    std::make_shared<Listener>(
        ioc,
#       ifdef HTTP_SERVER_ASYNC_FLEX
        ctx,
#       endif
        tcp::endpoint{address, port},
        std::make_shared<std::string const>(""))->run();

    // Capture SIGINT and SIGTERM to perform a clean shutdown
    boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait(
        [&](boost::system::error_code const&, int)
        {
            // Stop the `io_context`. This will cause `run()`
            // to return immediately, eventually destroying the
            // `io_context` and all of the sockets in it.
            ioc.stop();
        });

    // Run the I/O service on the requested number of threads
    std::vector<std::thread> v;
    v.reserve(threads - 1);
    for(auto i = threads - 1; i > 0; --i)
        v.emplace_back([&ioc] { ioc.run(); });
    ioc.run();

    // (If we get here, it means we got a SIGINT or SIGTERM)

    // Block until all the threads exit
    for(auto& t : v)
        t.join();

    log(info) << "webserver finished";

    return EXIT_SUCCESS;
}
