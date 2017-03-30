// -*- mode:C++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

/*
* Copyright (C) 2014 WYSIWYD Consortium, European Commission FP7 Project ICT-612139
* Authors: Hyung Jin Chang
* email:   hj.chang@imperial.ac.uk
* website: http://wysiwyd.upf.edu/
* Permission is granted to copy, distribute, and/or modify this program
* under the terms of the GNU General Public License, version 2 or any
* later version published by the Free Software Foundation.
*
* A copy of the license can be found at
* icub-client/license/gpl.txt
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
* Public License for more details
*/

/**
* @file main.cpp
* @brief main code.
*/

#include "faceTracker.h"

using namespace yarp::os;
using namespace yarp::sig;
using namespace yarp::sig::draw;
using namespace yarp::sig::file;
using namespace yarp::dev;
using namespace std;

int main(int argc, char * argv[]) {
    /* initialize yarp network */
    Network yarp;   // set up yarp

    if(!yarp.checkNetwork())
    {
        cout << "yarp network is not available for faceTracker!" << endl;
        return -1;
    }

    /* prepare and configure the resource finder */
    ResourceFinder rf;
    rf.setVerbose(true);
    rf.setDefaultConfigFile("faceTracker.ini"); //overridden by --from parameter
    rf.setDefaultContext("faceTracker");   //overridden by --context parameter
    rf.configure(argc, argv);

    /* create your module */
    faceTrackerModule module;

    /* run the module: runModule() calls configure first and, if successful, it then runs */
    yInfo() << "Start face tracking...";
    module.runModule(rf);

    return 0;
}

