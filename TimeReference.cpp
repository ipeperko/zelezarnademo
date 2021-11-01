#include "TimeReference.h"
#include "Application.h"

#include <iomanip>

void TimeReference::start(TimePoint start_time)
{
    do_run_ = false;
    if (thr_.joinable())
        thr_.join();

    pause_ = false;
    do_run_ = true;
    thr_ = std::thread([this, start_time]{ workerTask(start_time); });
    Application::instance().onSimulationChanged();
}

void TimeReference::stop()
{
    do_run_ = false;
    if (thr_.joinable())
        thr_.join();
    Application::instance().onSimulationChanged();
}

void TimeReference::pause(bool val)
{
    pause_ = val;
    Application::instance().onSimulationChanged();
}

void TimeReference::setSpeed(unsigned int s)
{
    speed_ = s;
    log(info) << "speed changed " << s;
    Application::instance().onSimulationChanged();
}

std::string TimeReference::status() const
{
    if (!do_run_)
        return "stopped";
    else if (pause_)
        return "paused";
    else
        return "running";
}

void TimeReference::registerPingCallback(void* handle, std::function<void(TimePoint)>&& cb)
{
    log(debug) << "Register callback " << handle;
    std::lock_guard lock(ping_callback_mtx_);
    ping_callback_[handle] = std::move(cb);
}

void TimeReference::unregisterPingCallback(void* handle)
{
    log(debug) << "Unregister callback " << handle;
    std::lock_guard lock(ping_callback_mtx_);
    if (auto p = ping_callback_.find(handle); p != ping_callback_.end())
        ping_callback_.erase(p);
}

std::string TimeReference::timeStamp(TimePoint tp)
{
    std::stringstream ss;
    std::time_t tp_c = std::chrono::system_clock::to_time_t(tp);
    double tp_ms = std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch()).count();
    tp_ms -= std::micro::den * tp_c;
    struct tm tm = {};
    gmtime_r(&tp_c, &tm);
    ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    ss << "." << std::setfill('0') << std::setw(6) << tp_ms << std::setfill(' ') << "Z";

    return ss.str();
}

void TimeReference::workerTask(TimePoint start_time)
{
    using namespace std::chrono_literals;

    log(info) << "task started";

    sim_time_ = start_time;
    auto ctrl_time = ClockType::now();

    while (do_run_) {
        double sleep_time = 1000;
        auto tp2 = ctrl_time + std::chrono::milliseconds(static_cast<int>(sleep_time));

        if (!pause_) {
            log(debug) << "ping - now : " << timeStamp(ClockType::now()) << " simulator time : "
                         << timeStamp(sim_time_);

            std::lock_guard lock(ping_callback_mtx_);
            for (auto &cb: ping_callback_)
                cb.second(sim_time_);
        }

        std::this_thread::sleep_until(tp2);
        ctrl_time = tp2;

        if (!pause_) {
            sim_time_ += std::chrono::seconds(speed_);
        }
    }

    log(info) << "task finished";
}