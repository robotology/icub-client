#ifndef GREETING_H
#define GREETING_H

#include <iostream>
#include <yarp/os/all.h>

#include "behavior.h"


class Greeting: public Behavior
{
private:
    void run(const yarp::os::Bottle &args);

public:
    Greeting(yarp::os::Mutex* mut, yarp::os::ResourceFinder &rf, std::string behaviorName): Behavior(mut, rf, behaviorName) {
        ;
    }

    void configure();
    void close_extra_ports() {
        ;
    }
};

#endif
