#ifndef WEBSERVER_COMMON_H
#define WEBSERVER_COMMON_H

#include <boost/beast.hpp>
#include <queue>
#include <boost/optional/optional.hpp>
#include <boost/algorithm/string.hpp>

#include "Log.h"

namespace beast = boost::beast;                 // from <boost/beast.hpp>
namespace http = beast::http;                   // from <boost/beast/http.hpp>
namespace websocket = beast::websocket;         // from <boost/beast/websocket.hpp>
namespace net = boost::asio;                    // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;               // from <boost/asio/ip/tcp.hpp>

class WebserverVersion
{
public:
    static std::string get()
    {
        return "zelezarna/1.0.0";
    }
};

#endif // WEBSERVER_COMMON_H
