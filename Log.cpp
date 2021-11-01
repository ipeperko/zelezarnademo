#include "Log.h"
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <iomanip>

namespace {

class ChannelLoglevel
{
    std::unordered_map<std::string_view, LoggerLevel> map_;
    std::unordered_set<std::string> keys_;
public:

    auto& operator[](std::string_view key)
    {
        auto it = map_.find(key);
        if (it == map_.end()) {
            keys_.insert(key.data());
            return map_[key.data()];
        }
        return it->second;
    }

    auto find(std::string_view key) { return map_.find(key); }

    auto find(std::string_view key) const { return map_.find(key); }

    auto begin() { return map_.begin(); }

    auto begin() const { return map_.begin(); }

    auto end() { return map_.end(); }

    auto end() const { return map_.end(); }
};

LoggerLevel global_loglevel = info;
ChannelLoglevel channel_loglevel;
//std::unordered_map<std::string, LoggerLevel> channel_loglevel;

} // namespace

Log::Log(std::string_view channel, LoggerLevel level)
    : level_(level)
{
    suppressed_ = isSuppressed(channel, level_);
    if (!suppressed_) {
        os_ << "[ " << std::left << std::setw(8) << levelTostring(level) << "]  ";
        os_ << "[ " << std::left << std::setw(16) << channel << " ]  ";
    }
}

Log::~Log()
{
    if (suppressed_)
        return;

    std::string msg = os_.str();
    if (msg.back() != '\n')
        msg += "\n";

    if (level_ <= error) {
        std::cout.flush();
        std::cerr << msg;
    }
    else {
        std::cout << msg;
    }

    std::cout.flush();
}

bool Log::isSuppressed(std::string_view channel, LoggerLevel level)
{
    LoggerLevel set_level;

    auto cl = channel_loglevel.find(channel.data());
    if (cl != channel_loglevel.end())
        set_level = cl->second;
    else
        set_level = global_loglevel;

    return level > set_level;
}

LoggerLevel Log::levelFromString(std::string_view level)
{
    if (level == "fatal")
        return fatal;
    else if (level == "error")
        return error;
    else if (level == "warning")
        return warning;
    else if (level == "info")
        return info;
    else if (level == "debug")
        return debug;
    else if (level == "trace")
        return trace;

    return info;
}

std::string_view Log::levelTostring(LoggerLevel level)
{
    switch (level) {
        case fatal: return "fatal";
        case error: return "error";
        case warning: return "warning";
        case info: return "info";
        case debug: return "debug";
        case trace: return "trace";
        default: return "?";
    }
}

void Log::setGlobalLoggingLevel(LoggerLevel level)
{
    global_loglevel = level;
}

void Log::setGlobalLoggingLevel(std::string_view level)
{
    global_loglevel = levelFromString(level);
}

void Log::setChannelLoggingLevel(std::string_view channel, LoggerLevel level)
{
    channel_loglevel[channel.data()] = level;
}

void Log::setChannelLoggingLevel(std::string_view channel, std::string_view level)
{
    channel_loglevel[channel.data()] = levelFromString(level);
}