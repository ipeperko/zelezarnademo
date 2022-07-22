#ifndef ZELEZARNA_APPLICATION_H
#define ZELEZARNA_APPLICATION_H

#include "Object.h"
#include "common.h"
#include "TimeReference.h"

#include <dbm/dbm.hpp>
#include <dbm/drivers/mysql/mysql_session.hpp>

#include <map>
#include <mutex>

class Daq;

class Application : public Object
{
private:
    Application();
public:
    struct MakePoolSession
    {
        std::shared_ptr<dbm::mysql_session> operator()()
        {
            return Application::instance().makeDbSession();
        }
    };

    using Pool = dbm::pool<dbm::mysql_session, MakePoolSession>;

    Application(Application const&) = delete;

    Application(Application&&) = delete;

    Application& operator=(Application const&) = delete;

    Application& operator=(Application&&) = delete;

    ~Application() override;

    static Application& instance();

    void init();

    auto& pool() { return pool_; }

    void cleanDatabase();

    void acceptMessage(std::string&& msg);

    void onSimulationChanged() const;

    std::string statusMessage() const;

    std::unique_lock<std::shared_mutex> dbLockUnique() { return std::unique_lock(db_mtx_); }

    std::shared_lock<std::shared_mutex> dbLockShared() { return std::shared_lock(db_mtx_); }

    auto& operationStatistics(std::string const& key) { return operation_statistics_[key]; }

    struct Options
    {
        std::string db_username;
        std::string db_password;
        std::string db_hostname {"127.0.0.1"};
        int db_port {3306};
    } options;

private:

    void sendStatisticsMessage() const;

    void onTimePing(TimePoint tp);

    void resetIterators();

    void resetStatistics();

    std::shared_ptr<dbm::mysql_session> makeDbSession() const;

    std::unique_ptr<Daq> daq_production_;
    std::unique_ptr<Daq> daq_energy_;

    Pool pool_;

    std::shared_mutex db_mtx_;
    TimePoint tp_initial_;
    TimePoint tp_kpi_last_; // time of the last kpi calculation
    TimePoint tp_kpi_next_; // time of the next kpi calculation

    unsigned calculation_id_ {0};
    std::map<std::string, OpearationStatistics> operation_statistics_;
};

#endif //ZELEZARNA_APPLICATION_H
