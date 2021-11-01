#include "WebsocketDataBus.h"
#include "WebsocketSession.h"
#include "nlohmann/json.hpp"

#include <mutex>

//
// DataBus implementation
//
WebsocketDataBus& WebsocketDataBus::instance()
{
    static WebsocketDataBus bus;
    return bus;
}

void WebsocketDataBus::registerSession(WebsocketSession* cli)
{
    log(debug) << "Registering new websocket session " << cli;
    std::unique_lock lock(mtx);
    sessions_.insert(cli);
}

void WebsocketDataBus::unregisterSession(WebsocketSession* cli)
{
    std::unique_lock lock(mtx);

    bool removed = [&]() -> bool {
        auto it = std::find(sessions_.begin(), sessions_.end(), cli);
        if (it != sessions_.end()) {
            sessions_.erase(it);
            return true;
        }
        return false;
    }();

    lock.unlock();

    if (removed) {
        log(debug) << "session " << cli << " unregistered";
    }
    else {
        log(error) << "Cannot unregister session " << cli << " - not found";
    }
}

void WebsocketDataBus::messageToWebclients(nlohmann::json&& j, bool admin_only)
{
    messageToWebclients(j.dump(), admin_only);
}

void WebsocketDataBus::messageToWebclients(std::string&& msg, bool admin_only)
{
    auto sm = std::make_shared<std::string const>(std::move(msg));

    std::vector<std::weak_ptr<WebsocketSession>> clients;
    {
        std::shared_lock lock(mtx);
        clients.reserve(sessions_.size());
        for_each_webclient_session([&] (auto* cli) {
            clients.emplace_back(cli->weak_from_this());
        });
    }

    for (auto& cli : clients)
        if (auto sp = cli.lock())
            sp->send(sm);
}

size_t WebsocketDataBus::countSessions() const
{
    std::shared_lock lock(mtx);
    return sessions_.size();
}


//// ----------------------------------------------------------------------
//// ---------------------- Private functions  ----------------------------
//// ---------------------- (shared mutex locks!)  ------------------------
//// ----------------------------------------------------------------------


//// ----------------------------------------------------------------------
//// ---------------------- Private functions  ----------------------------
//// ---------------------- (no mutex locks!)  ----------------------------
//// ----------------------------------------------------------------------

template<typename F>
void WebsocketDataBus::for_each_webclient_session(F&& f)
{
    for (auto& it : sessions_) {
        f(it);
    }
}
