#ifndef WEBSERVER_H
#define WEBSERVER_H

#include "Object.h"

class Webserver : public Object
{
public:

    Webserver()
        : Object("Webserver")
    {}

    int run(unsigned short port);
};

#endif

