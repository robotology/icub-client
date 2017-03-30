#include <string>
#include <iostream>
#include <iomanip>
#include <yarp/os/all.h>
#include <yarp/sig/all.h>
#include "wrdac/clients/icubClient.h"
#include <map>

#include "sensation.h"

using namespace std;
using namespace yarp::os;
using namespace yarp::sig;
using namespace yarp::math;
using namespace icubclient;ass OpcSensation: public Sensation
{
private:
    ICubClient *iCub;
    bool confusion;
    string moduleName, unknown_obj_port_name, confusion_port_name, show_port_name, known_obj_port_name, friendly_port_name, greeting_port_name;
    yarp::os::BufferedPort<Bottle> unknown_obj_port;
    yarp::os::BufferedPort<Bottle> confusion_port;
    yarp::os::BufferedPort<Bottle> friendly_port, greeting_port;

    yarp::os::BufferedPort<Bottle> known_obj_port;
    yarp::os::BufferedPort<Bottle> show_port;

    Bottle handleTagging();
    Bottle handlePointing();

public:

    void configure();
    void publish();

};