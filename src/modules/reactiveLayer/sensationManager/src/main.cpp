/* 
* Copyright (C) 2011 ICUBCLIENT Consortium, European Commission FP7 Project IST-270490
* Authors: Stephane Lallee
* email:   stephane.lallee@gmail.com
* website: https://github.com/robotology/icub-client/ 
* Permission is granted to copy, distribute, and/or modify this program
* under the terms of the GNU General Public License, version 2 or any
* later version published by the Free Software Foundation.
*
* A copy of the license can be found at
* $ICUBCLIENT_ROOT/license/gpl.txt
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
* Public License for more details
*/

/*

Handle the integration of all sensors and reactions of the robot.
The robot is reacting to tactile stimuli, gestures, voice and joystick inputs.
It also includes the subscenarios of Pon, DJ, and Tic Tac Toe.

*/

#include <yarp/os/all.h>
#include "sensationManager.h"

using namespace std;
using namespace yarp::os;

int main(int argc, char * argv[])
{
    srand(time(NULL));
    yarp::os::Network yarp;
    if (!yarp.checkNetwork())
    {
        yError()<<"YARP network seems unavailable!";
        return 1;
    }
    SensationManager mod;
    ResourceFinder rf;
    rf.setVerbose(true);
    rf.setDefaultContext("sensationManager");
    rf.setDefaultConfigFile("default.ini");
    rf.configure( argc, argv);
    return mod.runModule(rf);
}
