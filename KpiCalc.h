#ifndef ZELEZARNA_KPICALC_H
#define ZELEZARNA_KPICALC_H

#include "Object.h"
#include "TimeReference.h"

namespace dbm {
class mysql_session;
}

class KpiCalc : public Object
{
public:
    KpiCalc()
     : Object("KpiCalc")
    {}

    double calculateDaily(TimePoint tp, dbm::mysql_session& db);

    double calculateWeekly(TimePoint tp, dbm::mysql_session& db);

private:

    double calculate(dbm::mysql_session& db, TimePoint from, TimePoint to);
};

#endif //ZELEZARNA_KPICALC_H
