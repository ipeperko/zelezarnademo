#include "KpiCalc.h"
#include "Application.h"
#include "webserver/WebsocketDataBus.h"
#include "nlohmann/json.hpp"

using namespace std::chrono_literals;

namespace {

constexpr double invalidKPI = -1;

template<typename Tx, typename Ty>
Ty lin_interpolate(Tx x, Tx x1, Tx x2, Ty y1, Ty y2)
{
    return (y2 - y1) / (x2 - x1) * (x - x1) + y1;
}

} // namespace

double KpiCalc::calculateDaily(TimePoint tp, dbm::session& db)
{
    return calculate(db, tp - 24h, tp);
}

double KpiCalc::calculateWeekly(TimePoint tp, dbm::session& db)
{
    log(debug) << "Calculating weekly - time point " << TimeReference::timeStamp(tp);

    return calculate(db, tp - 7 * 24h, tp);
}

double KpiCalc::calculate(dbm::session& db, TimePoint from, TimePoint to)
{
    time_t tfrom = ClockType::to_time_t(from);
    time_t tto = ClockType::to_time_t(to);

    log(debug) << "Calculating kpi from " << TimeReference::timeStamp(from) << " to " << TimeReference::timeStamp(to) <<
        " (" << tfrom << " - " << tto << ") " <<
        " session " << std::hex << &db;

    auto energy_data = dbm::sql_rows_dump(db.select(
            (dbm::statement() << "CALL zelezarna.get_energy(" << tfrom << ", " << tto << ")").get() ));
    auto energy_data_rows = energy_data.restore();

    {
        auto lg = log(debug);
        lg << "energy data received " << energy_data_rows.size() << " entries";
        if (!energy_data_rows.empty()) {
            lg << " " << TimeReference::timeStamp(ClockType::from_time_t(energy_data_rows.front().at(1).get<time_t>()));
            lg << " - " << TimeReference::timeStamp(ClockType::from_time_t(energy_data_rows.back().at(1).get<time_t>()));
        }
    }

    if (energy_data_rows.size() < 2) {
        log(warning) << "cannot calculate kpi - no energy data";
        return invalidKPI;
    }

    double Esum = 0;
    auto it1 = energy_data_rows.begin();
    auto it2 = energy_data_rows.begin() + 1;

    for (; it2 != energy_data_rows.end(); ++it1, ++it2) {

        time_t t1 = it1->at(1).get<time_t>();
        time_t t2 = it2->at(1).get<time_t>();
        double E1 = it1->at(2).get_optional<double>(0);
        double E2 = it2->at(2).get_optional<double>(0);
        double E;

        if (it1 == energy_data_rows.begin()) {
            // first interval
            if (E2 < E1)
                E = E2;
            else if (t1 < tfrom)
                E = E2 - lin_interpolate(tfrom, t1, t2, E1, E2);
            else
                E = E2 - E1;
        }
        else if (it2 + 1 == energy_data_rows.end()) {
            // last interval
            if (E2 < E1)
                E = E2;
            else if (t2 > tto)
                E = lin_interpolate(tto, t1, t2, E1, E2) - E1;
            else
                E = E2 - E1;
        }
        else {
            // all the other intervals
            if (E2 < E1)
                E = E2;
            else
                E = E2 - E1;
        }

        Esum += E;
    }

    auto production_data = dbm::sql_rows_dump(db.select(
        (dbm::statement() << "CALL zelezarna.get_production(" << tfrom << ", " << tto << ")").get() ));
    auto production_data_rows = production_data.restore();

    if (production_data_rows.empty()) {
        log(warning) << "cannot calculate kpi - no production data";
        return invalidKPI;
    }

    auto P = std::accumulate(production_data_rows.begin(),
                             production_data_rows.end(),
                             0.0,
                             [](double sum, dbm::sql_row const& row) {
        return sum + row.at(2).get_optional<double>(0);
    });

    double kpi = (P > 0) ? (Esum / P) : invalidKPI;
    log(debug) << "Calculated Esum " << Esum << " / P " << P << " = KPI " << kpi;

    if (kpi > 30) {
        log(warning) << "kpi abnormal value - consider as invalid";
        kpi = invalidKPI;
    }

    return kpi;
}