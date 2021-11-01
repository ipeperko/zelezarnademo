#ifndef ZELEZARNA_OBJECT_H
#define ZELEZARNA_OBJECT_H

#include "Log.h"

class Object
{
public:
    explicit Object(std::string name)
        : name_(std::move(name))
    {}

    virtual ~Object() = default;

    auto const& name() const { return name_; }

    Log log(LoggerLevel level)
    {
        return Log(name_, level);
    }

private:
    std::string name_;
};

#endif //ZELEZARNA_OBJECT_H
