#ifndef ZELEZARNA_COMMON_H
#define ZELEZARNA_COMMON_H

#include <functional>

template<typename Func>
class Finally
{
    Func f;
public:
    explicit Finally(Func&& f) : f(std::move(f)) {}

    ~Finally()
    {
        f();
    }
};

// Daq operation statistics
struct OpearationStatistics
{
    unsigned long operations_count;                     // number of operations (e.g.: 1 op = open db and write data)
    unsigned long processing_exception_count;           // exceptions count during calculation
    unsigned long acquire_db_session_count;             // how many times acquired db connection from pool
    unsigned long acquire_db_session_failed_count;      // how many times failed to acquire db connection from pool
    unsigned long prepared_stmt_reuse_count;            // how many times prepared statement has been reused
    unsigned long records_to_write_count;               // how many records should be written
    unsigned long records_write_count;                  // how many records has been written
    unsigned long records_write_failed_count;           // how many records

    void reset()
    {
        *this = {};
    }
};

// Helper class for failures counting
template<typename T>
class CountIfNotCanceled
{
public:
    explicit CountIfNotCanceled(T& ref) : ref_(ref) {}

    ~CountIfNotCanceled()
    {
        if (!canceled_)
            ++ref_;
    }

    void cancel() { canceled_ = true; }

private:
    T& ref_;
    bool canceled_{false};
};


#endif //ZELEZARNA_COMMON_H
