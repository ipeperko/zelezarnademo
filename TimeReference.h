#ifndef ZELEZARNA_TIMEREFERENCE_H
#define ZELEZARNA_TIMEREFERENCE_H

#include "Object.h"
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <functional>

using ClockType = std::chrono::system_clock;
using TimePoint = ClockType::time_point;

class TimeReference : public Object
{
private:

    TimeReference()
        : Object("TimeReference")
    {}

public:

    TimeReference(const TimeReference&) = delete;

    TimeReference(TimeReference&&) = delete;

    TimeReference& operator=(const TimeReference&) = delete;

    TimeReference& operator=(TimeReference&&) = delete;

    static TimeReference& instance()
    {
        static TimeReference instance;
        return instance;
    }

    void start(TimePoint start_time);

    void stop();

    void pause(bool val);

    auto speed() const { return speed_; }

    void setSpeed(unsigned int s);

    auto simulatorCurrentTime() const { return sim_time_; }

    auto simulatorCurrentUnixtime() const { return ClockType::to_time_t(sim_time_); }

    std::string status() const;

    void registerPingCallback(void* handle, std::function<void(TimePoint)>&& cb);

    void unregisterPingCallback(void* handle);

    static std::string timeStamp(std::chrono::system_clock::time_point tp);

private:
    void workerTask(TimePoint start_time);

    unsigned int speed_ {1};
    std::chrono::milliseconds offset_;
    std::thread thr_;
    std::atomic<bool> do_run_;
    std::atomic<bool> pause_ {false};
    ClockType::time_point sim_time_;
    std::unordered_map<void*, std::function<void(TimePoint)>> ping_callback_;
    std::mutex mutable ping_callback_mtx_;
};


#endif //ZELEZARNA_TIMEREFERENCE_H
