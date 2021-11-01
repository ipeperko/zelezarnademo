#ifndef ZELEZARNA_KPICALC_H
#define ZELEZARNA_KPICALC_H

#include "Object.h"
#include "TimeReference.h"

namespace dbm {
class session;
}

class KpiCalc : public Object
{
public:
    KpiCalc()
     : Object("KpiCalc")
    {}

    void calculateDaily(TimePoint tp, dbm::session& db);

    void calculateWeekly(TimePoint tp, dbm::session& db);

private:

    double calculate(dbm::session& db, TimePoint from, TimePoint to);
};

#endif //ZELEZARNA_KPICALC_H
