#ifndef DUMMY_H
#define DUMMY_H

#include <iostream>
#include <yarp/os/all.h>

#include "behavior.h"


class Dummy: public Behavior
{
private:
    static int n_instances;

    void run(const yarp::os::Bottle &/*args*/) {
        yDebug() << "Dummy::run start " + behaviorName;
        yarp::os::Time::delay(4);
        yDebug() << "Dummy::run stop " + behaviorName;
    }
    int id;

public:
    Dummy(yarp::os::Mutex* mut, yarp::os::ResourceFinder &rf, std::string behaviorName): Behavior(mut, rf, behaviorName) {
        n_instances++;
        id = n_instances;
    }

    void configure() {
        ;
    }


    void close_extra_ports() {
        ;
    }

};

#endif
