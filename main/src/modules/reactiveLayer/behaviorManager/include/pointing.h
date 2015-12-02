#include <string>
#include <iostream>
#include <iomanip>
#include <yarp/os/all.h>
#include <yarp/sig/all.h>
#include <map>


#include "behavior.h"

using namespace std;
using namespace yarp::os;
using namespace yarp::sig;



class Pointing: public Behavior
{
public:
    void configure();
    void run(Bottle args=Bottle());
    void close_extra_ports() {
        ;
    }
};

