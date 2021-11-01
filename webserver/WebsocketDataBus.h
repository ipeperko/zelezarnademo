#ifndef WEBSOCKETDATABUS_H
#define WEBSOCKETDATABUS_H

#include "Object.h"
#include "nlohmann/json_fwd.hpp"
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <shared_mutex>
#include <unordered_set>

class WebsocketSession;

//
// DataBus class
//
class WebsocketDataBus : public Object
{
public:
    using session_list = std::unordered_set<WebsocketSession*>;

private:

    WebsocketDataBus()
        : Object("WebsocketDataBus")
    {}

public:
    WebsocketDataBus(const WebsocketDataBus&) = delete;

    WebsocketDataBus(WebsocketDataBus&&) = delete;

    WebsocketDataBus& operator=(const WebsocketDataBus&) = delete;

    WebsocketDataBus& operator=(WebsocketDataBus&&) = delete;

    static WebsocketDataBus& instance();

    void registerSession(WebsocketSession* cli);

    void unregisterSession(WebsocketSession* cli);

    void messageToWebclients(nlohmann::json&& j, bool admin_only=false);

    void messageToWebclients(std::string&& msg, bool admin_only=false);

    size_t countSessions() const;

private:

    template<typename F>
    void for_each_webclient_session(F&& f);

    session_list sessions_;
    std::shared_mutex mutable mtx;
};

#endif // WEBSOCKETDATABUS_H
