#include "Application.h"
#include "Daq.h"
#include "KpiCalc.h"
#include "webserver/WebsocketDataBus.h"
#include "nlohmann/json.hpp"

#include <dbm/drivers/mysql/mysql_session.hpp>

using namespace std::chrono_literals;

Application::Application()
    : Object("Application")
{
}

Application::~Application()
{
    TimeReference::instance().unregisterPingCallback(this);
}

Application& Application::instance()
{
    static Application app;
    return app;
}

void Application::init()
{
    daq_production_ = std::make_unique<DaqProduction>();
    daq_energy_ = std::make_unique<DaqEnergy>();

    auto minmax = std::minmax(daq_energy_->data().begin()->tp, daq_production_->data().begin()->tp);

    tp_initial_ = minmax.second + 14 * 24h;
    resetIterators();

    dbm::utils::debug_logger::writer = [](auto level, auto&& msg) {
        if (level == dbm::utils::debug_logger::level::Error) {
            Log("dbm", error) << msg;
        }
        else {
            Log("dbm", debug) << msg;
        }
    };

    pool_.set_max_connections(10);
    pool_.set_session_initializer([]() {
        return Application::instance().makeDbSession();
    });

    try {
        cleanDatabase();
    }
    catch (std::exception&) {
        throw;
    }

    TimeReference::instance().registerPingCallback(this, std::bind(&Application::onTimePing, this, std::placeholders::_1));
}

void Application::cleanDatabase()
{
    auto conn = pool_.acquire();
    conn.get().query("DELETE FROM energy_data");
    conn.get().query("DELETE FROM production_data");
}

void Application::acceptMessage(std::string&& msg)
{
    auto& timeref = TimeReference::instance();

    try {
        auto j = nlohmann::json::parse(msg);

        if (j.find("command") != j.end()) {
            auto cmd = j["command"];
            auto type = cmd["type"];

            if (type == "start") {
                resetIterators();
                timeref.stop();
                try {
                    cleanDatabase();
                }
                catch (std::exception&) {
                    log(error) << "Clean database failed";
                }

                static unsigned calc_id = 0;
                calculation_id_ = calc_id++;
                timeref.start(tp_initial_);
            }
            else if (type == "stop") {
                timeref.stop();
            }
            else if (type == "pause") {
                timeref.pause(true);
            }
            else if (type == "resume") {
                timeref.pause(false);
            }
            else if (type == "speed") {
                timeref.setSpeed(cmd.at("value").get<unsigned>());
            }
            else if (type == "reset_statistics") {
                resetStatistics();
            }
            else if (type == "get_statistics") {
                sendStatisticsMessage();
            }
            else if (type == "global_logging_level") {
                Log::setGlobalLoggingLevel(cmd.at("level").get<std::string>());
            }
            else if (type == "channel_logging_level") {
                Log::setChannelLoggingLevel(
                        cmd.at("channel").get<std::string>(),
                        cmd.at("level").get<std::string>());
            }
        }
        else {
            throw std::runtime_error("Command not recognized");
        }
    }
    catch (std::exception& e) {
        log(error) << "acceptMessage error : " << e.what() << " '" << msg << "'";
    }
}

void Application::onSimulationChanged() const
{
    WebsocketDataBus::instance().messageToWebclients(statusMessage());
}

std::string Application::statusMessage() const
{
    auto& timeref = TimeReference::instance();
    nlohmann::json j;
    j["sim_time"] = timeref.simulatorCurrentUnixtime();
    j["sim_status"] = timeref.status();
    j["sim_speed"] = timeref.speed();

    return j.dump(4);
}

void Application::sendStatisticsMessage() const
{    
    auto statistics = operation_statistics_; // make a copy
    auto pool_stat = pool_.stat();

    nlohmann::json profile = nlohmann::json::array();
    for (auto const& it : pool_stat.acquire_stat) {
        profile.push_back({ std::to_string(it.first) + "ms", it.second });
    }

    nlohmann::json json;
    json["operation_statistics"]["pool"] = {
            { "n_conn", pool_stat.n_conn },
            { "n_active_conn", pool_stat.n_active_conn },
            { "n_idle_conn", pool_stat.n_idle_conn },
            { "n_heartbeats", pool_stat.n_heartbeats },
            { "n_acquired", pool_stat.n_acquired },
            { "n_acquiring", pool_stat.n_acquiring },
            { "n_acquiring_max", pool_stat.n_acquiring_max },
            { "n_max_conn", pool_stat.n_max_conn },
            { "n_timeouts", pool_stat.n_timeouts },
            { "acquire_profile", std::move(profile)}
    };

    json["operation_statistics"]["daq"] = nlohmann::json::array();

    for (auto const& stat : statistics) {
        nlohmann::json node;
        node["item_name"] = stat.first;
        node["operations_count"] = stat.second.operations_count;
        node["processing_exception_count"] = stat.second.processing_exception_count;
        node["acquire_db_session_count"] = stat.second.acquire_db_session_count;
        node["acquire_db_session_failed_count"] = stat.second.acquire_db_session_failed_count;
        node["prepared_stmt_reuse_count"] = stat.second.prepared_stmt_reuse_count;
        node["records_to_write_count"] = stat.second.records_to_write_count;
        node["records_write_count"] = stat.second.records_write_count;
        node["records_write_failed_count"] = stat.second.records_write_failed_count;
        json["operation_statistics"]["daq"].push_back(std::move(node));
    }

    WebsocketDataBus::instance().messageToWebclients(std::move(json));
}

void Application::onTimePing(TimePoint tp)
{
    std::thread([this, tp] {

        WebsocketDataBus::instance().messageToWebclients(nlohmann::json{
            {"sim_time", ClockType::to_time_t(tp)},
            {"calc_id", calculation_id_}
        });

        auto thr1 = std::thread([this, tp] {
            daq_energy_->pingSlot(tp);
        });

        auto thr2 = std::thread([this, tp] {
            daq_production_->pingSlot(tp);
        });

        thr1.join();
        thr2.join();

        if (tp >= tp_kpi_next_) {
            //auto db = pool_.acquire(); // get session from pool
            //Finally finally([&] {
            //    db.get().close(); // TODO: if not closed prepared statements from another threads may fail
            //});

            auto db = makeDbSession(); // session not from pool

            try {
                double kpi = KpiCalc().calculateDaily(tp_kpi_next_, *db);

                WebsocketDataBus::instance().messageToWebclients(nlohmann::json {
                        {"kpi_daily", {
                                { "time", ClockType::to_time_t(tp) },
                                { "value", kpi },
                                { "calc_id", calculation_id_ }
                        }
                        }});
            }
            catch (std::exception& e) {
                log(error) << "kpi calculate daily failed : " << e.what();
            }

            time_t utime = ClockType::to_time_t(tp_kpi_next_);
            struct tm timeinfo = {};
            gmtime_r(&utime, &timeinfo);

            if (timeinfo.tm_wday == 0) {
                // every sunday
                try {
                    double kpi = KpiCalc().calculateWeekly(tp_kpi_next_, *db);

                    WebsocketDataBus::instance().messageToWebclients(nlohmann::json {
                            {"kpi_weekly", {
                                    { "time", ClockType::to_time_t(tp) },
                                    { "value", kpi },
                                    { "calc_id", calculation_id_ }
                            }
                            }});
                }
                catch (std::exception& e) {
                    log(error) << "kpi calculate weekly failed : " << e.what();
                }
            }

            tp_kpi_last_ = tp_kpi_next_;
            tp_kpi_next_ += 24h;
        }

    }).detach();
}

void Application::resetIterators()
{
    resetStatistics();

    daq_energy_->resetIterator(tp_initial_);
    daq_production_->resetIterator(tp_initial_);

    tp_kpi_last_ = {};

    time_t utime = ClockType::to_time_t(tp_initial_);
    struct tm timeinfo = {};
    gmtime_r(&utime, &timeinfo);
    timeinfo.tm_hour = 6;
    timeinfo.tm_min = 0;
    timeinfo.tm_sec = 0;
    timeinfo.tm_isdst = -1;
    utime = mktime(&timeinfo);
    utime += timeinfo.tm_gmtoff;
    tp_kpi_next_ = ClockType::from_time_t(utime);

    log(debug) << "Next kpi calculation time " << TimeReference::timeStamp(tp_kpi_next_);
}

void Application::resetStatistics()
{
    pool_.reset_heartbeats_counter();

    for (auto& it : operation_statistics_) {
        it.second.reset();
    }
}

std::shared_ptr<dbm::session> Application::makeDbSession() const
{
    auto conn = std::make_shared<dbm::mysql_session>();
    conn->connect(options.db_hostname, options.db_username, options.db_password, "zelezarna", options.db_port);
    return conn;
}
