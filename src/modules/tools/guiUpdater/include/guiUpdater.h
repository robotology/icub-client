/* 
 * Copyright (C) 2014 WYSIWYD Consortium, European Commission FP7 Project ICT-612139
 * Authors: Stéphane Lallée
 * email:   stephane.lallee@gmail.com
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

#ifndef __GUIUPV2_H__
#define __GUIUPV2_H__

#include <string>
#include <yarp/os/all.h>
#include <yarp/sig/all.h>
#include <wrdac/clients/opcClient.h>

using namespace std;
using namespace yarp::os;
using namespace yarp::sig;
using namespace icubclient;

class GuiUpdater: public RFModule
{
private:
    OPCClient *opc;
    Agent* iCub;
    Port handlerPort;      //a port to handle messages 
    Port toGui;
    Port toGuiBase;
    list<shared_ptr<Entity>> oldEntities;
    bool displaySkeleton;

public:
    bool configure(yarp::os::ResourceFinder &rf); // configure all the module parameters and return true if successful
    bool interruptModule();                       // interrupt, e.g., the ports 
    bool close();                                 // close and shut down the module
    bool respond(const yarp::os::Bottle& command, yarp::os::Bottle& reply);
    double getPeriod(); 
    bool updateModule();
    void resetGUI();
    void deleteObject(const string &opcTag, Object* o = NULL);
    void addObject(Object* o, const string &opcTag);
    void addAgent(Agent* a, const string &opcTag);
    void addDrives(Agent* a);
    void moveBase(Agent* a);
    bool isDisplayable(Entity* entity);
    Vector getDriveColor(const Drive &d);
};

#endif


