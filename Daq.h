#ifndef ZELEZARNA_DAQ_H
#define ZELEZARNA_DAQ_H

#include "Object.h"
#include "TimeReference.h"
#include <chrono>
#include <string>
#include <vector>

enum class DaqType {
    energy,
    production
};


// Daq class
class Daq : public Object
{
public:

    struct Data
    {
        TimePoint tp;
        std::string tps; // time string (develop only)
        double val {0.0};
        bool is_null {false};
    };

    static std::string typeToString(DaqType t) {
        switch (t) {
            case DaqType::energy: return "electrical";
            case DaqType::production: return "production";
            default: return "unknown";
        }
    }


    explicit Daq(std::string name);

    virtual DaqType type() const = 0; // { return type_; }

    void pingSlot(TimePoint tp);

    void resetIterator(TimePoint tp);

    bool isIteratorValid() const { return data_.size() && iterator_ != data_.end(); }

    TimePoint iteratorTimePoint() const;

    auto const& data() const { return data_; }

protected:
    void getData();

    void insertData(std::vector<Data> const& pts) noexcept;

    virtual std::string_view insertStatement() const = 0;

    std::vector<Data> data_;
    std::vector<Data>::const_iterator iterator_;
};


class DaqEnergy : public Daq
{
public:
    DaqEnergy()
        : Daq("DaqEnergy")
    {
        getData();
    }

    DaqType type() const override { return DaqType::energy; }

    std::string_view insertStatement() const override { return "SELECT upsert_energy(?, ?)"; }
};


class DaqProduction : public Daq
{
public:
    DaqProduction()
        : Daq("DaqProduction")
    {
        getData();
    }

    DaqType type() const override { return DaqType::production; }

    std::string_view insertStatement() const override { return "SELECT upsert_production(?, ?)"; }
};


#endif //ZELEZARNA_DAQ_H
