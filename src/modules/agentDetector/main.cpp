/*
 * Copyright (C) 2014 WYSIWYD Consortium, European Commission FP7 Project ICT-612139
 * Authors: Stéphane Lallée, moved from ICUBCLIENT by Maxime Petit
 * email:   stephane.lallee@gmail.com
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

#include <yarp/os/Network.h>
#include "AgentDetector.h"
using namespace yarp::os;
int main(int argc, char *argv[])
{
    Network yarp;

    if (!yarp.checkNetwork())
    {
        fprintf(stdout, "Yarp network not available\n");
        return -1;
    }

    ResourceFinder rf;
    rf.setVerbose(true);
    rf.setDefaultContext("agentDetector");
    rf.setDefaultConfigFile("agentDetector.ini");
    rf.configure(argc,argv);

    AgentDetector mod;

    mod.runModule(rf);

    return 0;
}

