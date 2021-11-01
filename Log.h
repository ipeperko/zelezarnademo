#ifndef ZELEZARNA_LOG_H
#define ZELEZARNA_LOG_H

#include <sstream>

enum LoggerLevel
{
    fatal,
    error,
    warning,
    info,
    debug,
    trace
};

class Log
{
public:
    explicit Log(std::string_view channel, LoggerLevel level);

    ~Log();

    template<typename T>
    Log& operator<<(T const& val)
    {
        if (!suppressed_) {
            os_ << val;
        }
        return *this;
    }

    static bool isSuppressed(std::string_view channel, LoggerLevel level);

    static LoggerLevel levelFromString(std::string_view level);

    static std::string_view levelTostring(LoggerLevel level);

    static void setGlobalLoggingLevel(LoggerLevel level);

    static void setGlobalLoggingLevel(std::string_view level);

    static void setChannelLoggingLevel(std::string_view channel, LoggerLevel level);

    static void setChannelLoggingLevel(std::string_view channel, std::string_view level);

private:
    std::ostringstream os_;
    LoggerLevel level_;
    bool suppressed_;
};


#endif //ZELEZARNA_LOG_H
