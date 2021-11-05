#include "Daq.h"
#include "TimeReference.h"
#include "Application.h"

#include <dbm/dbm.hpp>

#include <fstream>
#include <array>
#include <iomanip>
#include <random>

#define RANDOM_SLEEP_TEST

using namespace std::chrono_literals;

namespace {
int dataCodeByDaqType(DaqType type)
{
    switch (type) {
        case DaqType::energy: return 3016;
        case DaqType::production: return 6008;
        default: return 0;
    }
}

auto acquire_pool_connection_helper(OpearationStatistics& stat)
{
    CountIfNotCanceled count_db_acquire_fail(stat.acquire_db_session_failed_count);
    auto conn = Application::instance().pool().acquire();
    stat.acquire_db_session_count++;
    count_db_acquire_fail.cancel();
    return conn;
}

template<typename MapType>
auto get_values_from_map(MapType const& map)
{
    std::vector<typename MapType::mapped_type> vec;
    vec.reserve(map.size());
    for (auto const& it : map)
        vec.push_back(it.second);
    return vec;
}

void query_helper(dbm::prepared_stmt& stmt, dbm::session& db, OpearationStatistics& stat)
{
    bool was_null = stmt.native_handle() == nullptr;
    auto handles = get_values_from_map(db.prepared_statement_handles());
    db.query(stmt);
    if (was_null && handles.end() != std::find(handles.begin(), handles.end(), stmt.native_handle())) {
        stat.prepared_stmt_reuse_count++;
    }
}
} // namespace

Daq::Daq(std::string name)
    : Object(std::move(name))
{
}

void Daq::pingSlot(TimePoint tp)
{
    try {
        std::vector<Data> pts;

        log(trace) << "pingSlot " << this;

        while (tp >= iterator_->tp && iterator_ != data_.end()) {
            log(trace) << "Trigger sim time : " << TimeReference::timeStamp(tp) << " data time : "
                       << TimeReference::timeStamp(iterator_->tp);
            pts.push_back(*iterator_);
            ++iterator_;
        }

        if (!pts.empty())
            insertData(pts);
    }
    catch (std::exception& e) {
        log(error) << "pingSlot exception : " << e.what();
    }
}

void Daq::resetIterator(TimePoint tp)
{
    log(info) << "initializing iterator to " << TimeReference::timeStamp(tp);

    iterator_ = data_.begin();

    while (tp > iterator_->tp && iterator_ != data_.end()) {
        ++iterator_;
    }

    if (iterator_ != data_.end()) {
        log(debug) << "iterator initialized to " << TimeReference::timeStamp(iterator_->tp);
    }
    else {
        throw std::runtime_error(name() + " iterator initialize failed");
    }
};

TimePoint Daq::iteratorTimePoint() const
{
    if (!isIteratorValid())
        throw std::runtime_error(name() + " daq iterator not valid");
    else
        return iterator_->tp;
}

void Daq::getData()
{
    data_.clear();

    // Open file
    std::string filename("data/KpiData.txt");
    std::ifstream fin(filename);

    if (!fin.is_open())
        throw std::runtime_error(name() +" cannot open file " + filename + " for reading");

    // Read file line by line
    std::string line;
    int code_filter = dataCodeByDaqType(type());
    std::vector<Data> data_tmp;

    while (std::getline(fin, line)) {

        int code = 0;
        std::array<char, 64> date = {};
        std::array<char, 64> time = {};
        double val = 0;
        bool is_null = false;

        int n = sscanf(line.c_str(), "%d %s %s %lf", &code, date.data(), time.data(), &val);

        if (n < 3) {
            log(error) << "data format error - n parsed " << n << " line: " << line.substr(0, line.size()-1);
            continue;
        }

        if (code != code_filter)
            continue;

        if (n < 4) {
            log(debug) << "value is null - n parsed " << n << " line: " << line.substr(0, line.size()-1);
            is_null = true;
        }

        std::tm tm = {};
        std::string date_time = std::string(date.data()) + " " + std::string(time.data());
        std::stringstream ss(date_time);
        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S"); // this will convert to local time
        tm.tm_isdst = -1;
        std::time_t t = std::mktime(&tm);
        t += tm.tm_gmtoff;
        auto tp = ClockType::from_time_t(t);
        log(trace) << date_time << " --> " << TimeReference::timeStamp(tp) << " (" << t << " " << ClockType::to_time_t(tp) << ") " << " val " << val << " null " << is_null;

        data_tmp.emplace_back(Data{tp, date_time, val, is_null});
    }

    // data reverse
    data_.reserve(data_tmp.size());
    std::copy(data_tmp.rbegin(), data_tmp.rend(), std::back_inserter(data_));

    // initialize iterator
    iterator_ = data_.begin();
}

void Daq::insertData(std::vector<Data> const& pts) noexcept
{
    auto &stat = Application::instance().operationStatistics(name());
    stat.operations_count++;
    stat.records_to_write_count += pts.size();

    try {
#ifdef RANDOM_SLEEP_TEST
        // perform random sleep to test connection acquire
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(1, 50);
        std::this_thread::sleep_for(std::chrono::milliseconds(distrib(gen)));
#endif

        auto conn = acquire_pool_connection_helper(stat);

        {
            auto lg = log(debug);
            lg << "inserting       " << pts.size() << " pts ";
            if (!pts.empty()) {
                lg << TimeReference::timeStamp(pts.front().tp) << " - "
                    << TimeReference::timeStamp(pts.back().tp) << " ";
                lg << "session " << std::hex << &conn.get()
                    << " num stmt handles " << std::dec << conn.get().prepared_statement_handles().size();
            }
        }

        dbm::prepared_stmt stmt(insertStatement().data(),
                                dbm::local<time_t>(),
                                dbm::local<double>());
        unsigned long count = 0;
        unsigned long count_failed = 0;

        for (auto const &it: pts) {
            stmt.param(0)->set(std::chrono::system_clock::to_time_t(it.tp));
            stmt.param(1)->set(it.val);
            stmt.param(1)->set_null(it.is_null);
            try {
                query_helper(stmt, *conn, stat);
                count++;
            } catch (std::exception &e) {
                count_failed++;
                stat.processing_exception_count++;
                log(error) << "insert data failed : " << e.what();
            }
        }

        stat.records_write_count += count;
        stat.records_write_failed_count += count_failed;

        {
            auto lg = log(debug);
            lg << "insert finished " << count << "/" << pts.size() << " pts succeeded ";
            if (!pts.empty()) {
                lg << TimeReference::timeStamp(pts.front().tp) << " - "
                    << TimeReference::timeStamp(pts.back().tp) << " session " << std::hex << &conn.get();
            }
        }
    }
    catch(std::exception& e) {
        stat.processing_exception_count++;
        log(error) << e.what();
    }
}

