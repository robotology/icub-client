/* 
 * Copyright (C) 2014 WYSIWYD Consortium, European Commission FP7 Project ICT-612139
 * Authors: Stéphane Lallée
 * email:   stephane.lallee@gmail.com
 * Permission is granted to copy, distribute, and/or modify this program
 * under the terms of the GNU General Public License, version 2 or any
 * later version published by the Free Software Foundation.
 *
 * A copy of the license can be found at
 * wysiwyd/license/gpl.txt
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details
*/

#include <yarp/os/all.h>
#include <yarp/sig/all.h>
#include <yarp/dev/all.h>
#include <iostream>
#include <string>
#include <map>
#include <list>

#include "guiUpdater.h"

using namespace std;
using namespace yarp;
using namespace yarp::sig;
using namespace yarp::os;
using namespace yarp::dev;


bool GuiUpdater::configure(yarp::os::ResourceFinder &rf)
{
    string moduleName      = rf.check("name",
                                      Value("guiUpdater"),
                                      "module name (string)").asString().c_str();

    string opcName      = rf.check("OPCname",
                                   Value("OPC"),
                                   "OPC name (string)").asString().c_str();

    setName(moduleName.c_str());

    displaySkeleton = rf.check("displaySkeletons");
    cout<<"Display skeleton status: "<<displaySkeleton<<endl;

    opc = new OPCClient(getName().c_str());
    opc->connect(opcName);
    opc->isVerbose = false;
    if(opc->isConnected()) {
        iCub = opc->addOrRetrieveEntity<Agent>("icub");
    }

    //GUI Port
    string guiPortName = "/";
    guiPortName +=  getName() + "/gui:o";
    toGui.open(guiPortName.c_str());
    resetGUI();

    //GUI Base Port
    string guiBasePortName = "/";
    guiBasePortName +=  getName() + "/guiBase:o";
    toGuiBase.open(guiBasePortName.c_str());

    //RPC
    string handlerPortName = "/";
    handlerPortName +=  getName() + "/rpc";
    if (!handlerPort.open(handlerPortName.c_str())) {
        cout << getName() << ": Unable to open port " << handlerPortName << endl;
        return false;
    }
    attach(handlerPort);                  // attach to port

    return true ;
}

bool GuiUpdater::interruptModule()
{
    yDebug() << "interrupt";
    opc->interrupt();
    toGui.interrupt();
    toGuiBase.interrupt();
    handlerPort.interrupt();
    yDebug() << "interrupt done";
    return true;
}

bool GuiUpdater::close()
{
    yDebug() << "close";
    if(opc) {
        opc->close();
        delete opc;
    }
    if(iCub) {
        delete iCub;
    }

    toGui.interrupt();
    toGui.close();
    toGuiBase.interrupt();
    toGuiBase.close();

    handlerPort.interrupt();
    handlerPort.close();

    yDebug() << "bye";

    return true;
}

bool GuiUpdater::respond(const yarp::os::Bottle& command, yarp::os::Bottle& reply)
{  
    string helpMessage =  string(getName().c_str()) +
            " commands are: \n" +
            "reset \n" +
            "help \n" +
            "quit \n" ;

    reply.clear();

    if (command.get(0).asString()=="quit") {
        reply.addString("quitting");
        return false;
    }
    else if (command.get(0).asString()=="help") {
        cout << helpMessage;
        reply.addString("ok");
    }
    else if (command.get(0).asString()=="reset") {
        cout << "Reset"<<endl;
        resetGUI();
        reply.addString("ok");
    }
    return true;
}

double GuiUpdater::getPeriod()
{
    return 0.1;
}

bool GuiUpdater::updateModule()
{
    if (opc->isConnected())
    {
        //Retrieve every entities
        opc->checkout();

        //Just add the iCub in case it's not there
        iCub = opc->addOrRetrieveEntity<Agent>("icub");

        //cout<<"iCub Position: \t"<<iCub->m_ego_position.toString(3,3)<<endl
        //    <<"iCub Orientation: \t"<<iCub->m_ego_orientation.toString(3,3)<<endl;

        //Display the iCub specifics
        moveBase(iCub);
        addDrives(iCub);

        //Display the objects
        list<shared_ptr<Entity>> entities = opc->EntitiesCacheCopy();
        if (oldEntities.size() != entities.size()) {
            resetGUI();
        } else {
            list<shared_ptr<Entity>>::iterator e_new, e_old;
            for(e_new = entities.begin(), e_old = oldEntities.begin();
                e_new != entities.end() && e_old != oldEntities.end();
                e_new++, e_old++)
            {
                if((*e_new)->name() != (*e_old)->name()) {
                    resetGUI();
                    break;
                }
            }
        }
        oldEntities = entities;

        for(auto& entity : entities)
        {
            if( isDisplayable(entity.get()) )
            {
                Object* o = dynamic_cast<Object*>(entity.get());
                if(!o) {
                    yError() << "Could not cast " << entity->name();
                    continue;
                }

                ostringstream guiTag;
                guiTag<< o->name() <<"("<<o->opc_id()<<")";

                if (o->m_present==0.0) {
                    deleteObject(guiTag.str(), o);
                }
                else
                {
                    if (o->name() != "icub")
                    {
                        if (o->isType(ICUBCLIENT_OPC_ENTITY_AGENT))
                        {
                            addAgent(dynamic_cast<Agent*>(o), guiTag.str());
                        }
                        else
                        {
                            addObject(o,guiTag.str());
                        }
                    }
                }
            }
        }
    }

    return true;
}

bool GuiUpdater::isDisplayable(Entity* entity)
{
    return entity->isType(ICUBCLIENT_OPC_ENTITY_OBJECT);
}

void GuiUpdater::deleteObject(const string &opcTag, Object* o)
{
    Bottle cmd;
    cmd.addString("delete");
    cmd.addString(opcTag.c_str());
    toGui.write(cmd);

    //Delete all the body parts

    if (o != NULL && o->entity_type() == ICUBCLIENT_OPC_ENTITY_AGENT && displaySkeleton)
    {
        unsigned int i = 0;
        Agent* a = dynamic_cast<Agent*>(o);
        while (i < a->m_body.m_parts.size())
        {
            ostringstream opcTagPart;
            opcTagPart << o->opc_id() << "_" << i;
            cmd.clear();
            cmd.addString("delete");
            cmd.addString(opcTagPart.str().c_str());
            toGui.write(cmd);
            i++;
        }
    }

    //delete all the drives
    if (o != NULL && o->entity_type() == ICUBCLIENT_OPC_ENTITY_AGENT)
    {
        Agent* a = dynamic_cast<Agent*>(o);
        for(auto& drive : a->m_drives)
        {
            ostringstream opcTagDrive;
            opcTagDrive<<a->name()<<"_"<<drive.second.name;
            cmd.clear();
            cmd.addString("delete");
            cmd.addString(opcTagDrive.str().c_str());
            toGui.write(cmd);
        }
    }
}

void GuiUpdater::addAgent(Agent* o, const string &opcTag)
{
    addDrives( o );
    if (displaySkeleton)
    {
        int i=0;
        for(auto& part : o->m_body.m_parts)
        {
            //Get the position of the object in the current reference frame of the robot (not the initial one)
            Vector inCurrentRootReference = iCub->getSelfRelativePosition(part.second);
            //cout<<o->name()<<" init Root: \t \t"<<o->m_ego_position.toString(3,3)<<endl
            //    <<o->name()<<" current Root: \t \t"<<inCurrentRootReference.toString(3,3)<<endl;

            ostringstream opcTagPart;
            opcTagPart<<o->opc_id() << "_" << i;
            Bottle cmd;
            cmd.addString("object");
            cmd.addString(opcTagPart.str().c_str());

            //Body parts have fixed dimensions
            cmd.addDouble(0.05 *1000.0);    // dimX in [mm]
            cmd.addDouble(0.05 *1000.0);    // dimY in [mm]
            cmd.addDouble(0.05 *1000.0);    // dimZ in [mm]
            cmd.addDouble(inCurrentRootReference[0] *1000.0);        // posX in [mm]
            cmd.addDouble(inCurrentRootReference[1] *1000.0);        // posY in [mm]
            cmd.addDouble(inCurrentRootReference[2] *1000.0);        // posZ in [mm]
            cmd.addDouble(0);             // discard the orientation
            cmd.addDouble(0);             // "
            cmd.addDouble(0);             // "
            cmd.addInt((int)o->m_color[0]);            // color R
            cmd.addInt((int)o->m_color[1]);            // color G
            cmd.addInt((int)o->m_color[2]);            // color B
            cmd.addDouble(1);                     // alpha coefficient [0,1]
            toGui.write(cmd);
            i++;
        }
    }
    else
    {
        //Vector inCurrentRootReference = iCub->getSelfRelativePosition(o->m_body.m_parts[ICUBCLIENT_OPC_BODY_PART_TYPE_HEAD]);
        Vector inCurrentRootReference = iCub->getSelfRelativePosition(o->m_ego_position);
        ostringstream opcTagPart;
        opcTagPart<< o->name() <<"("<<o->opc_id()<<")";
        Bottle cmd;
        cmd.addString("object");
        cmd.addString(opcTagPart.str().c_str());

        //Body parts have fixed dimensions
        cmd.addDouble(0.15 *1000.0);    // dimX in [mm]
        cmd.addDouble(0.15 *1000.0);    // dimY in [mm]
        cmd.addDouble(0.25 *1000.0);    // dimZ in [mm]
        cmd.addDouble(inCurrentRootReference[0]  *1000.0);        // posX in [mm]
        cmd.addDouble(inCurrentRootReference[1]  *1000.0);        // posY in [mm]
        cmd.addDouble(inCurrentRootReference[2]  *1000.0);        // posZ in [mm]
        cmd.addDouble(0 - iCub->m_ego_orientation[0]);             // discard the orientation
        cmd.addDouble(0 - iCub->m_ego_orientation[1]);             // "
        cmd.addDouble(0 - iCub->m_ego_orientation[2]);             // "
        cmd.addInt((int)o->m_color[0]);            // color R
        cmd.addInt((int)o->m_color[1]);            // color G
        cmd.addInt((int)o->m_color[2]);            // color B
        cmd.addDouble(1);                     // alpha coefficient [0,1]
        toGui.write(cmd);
    }
}

void GuiUpdater::addObject(Object* o, const string &opcTag)
{
    //Get the position of the object in the current reference frame of the robot (not the initial one)
    Vector inCurrentRootReference = iCub->getSelfRelativePosition(o->m_ego_position);
    //cout<<o->name()<<" init Root: \t \t"<<o->m_ego_position.toString(3,3)<<endl
    //    <<o->name()<<" current Root: \t \t"<<inCurrentRootReference.toString(3,3)<<endl;

    Bottle cmd;
    cmd.addString("object");
    cmd.addString(opcTag.c_str());

    cmd.addDouble(o->m_dimensions[0] *1000.0);    // dimX in [mm]
    cmd.addDouble(o->m_dimensions[1] *1000.0);    // dimY in [mm]
    cmd.addDouble(o->m_dimensions[2] *1000.0);    // dimZ in [mm]
    cmd.addDouble(inCurrentRootReference[0] *1000.0);        // posX in [mm]
    cmd.addDouble(inCurrentRootReference[1] *1000.0);        // posY in [mm]
    cmd.addDouble(inCurrentRootReference[2] *1000.0);        // posZ in [mm]
    cmd.addDouble(o->m_ego_orientation[0] - iCub->m_ego_orientation[0]);             // Deal with the object orientation that is moving with the base
    cmd.addDouble(o->m_ego_orientation[1] - iCub->m_ego_orientation[1]);             // "
    cmd.addDouble(o->m_ego_orientation[2] - iCub->m_ego_orientation[2]);              // "
    cmd.addInt((int)o->m_color[0]);            // color R
    cmd.addInt((int)o->m_color[1]);            // color G
    cmd.addInt((int)o->m_color[2]);            // color B
    cmd.addDouble(1);                     // alpha coefficient [0,1]
    toGui.write(cmd);
}

void GuiUpdater::moveBase(Agent* a)
{
    Bottle cmd;
    cmd.addDouble(a->m_ego_orientation[0]); //in opc we store rotation around x,y,z, which seems different of the iCubGUI
    cmd.addDouble(a->m_ego_orientation[1]);
    cmd.addDouble(a->m_ego_orientation[2]);
    cmd.addDouble(a->m_ego_position[0] * 1000.0);
    cmd.addDouble(a->m_ego_position[1] * 1000.0);
    cmd.addDouble(a->m_ego_position[2] * 1000.0);
    toGuiBase.write(cmd);
}

void GuiUpdater::addDrives(Agent* a)
{
    Vector driveDimension(3);
    driveDimension[0] = 10;
    driveDimension[1] = 500;
    driveDimension[2] = 10;

    //Create drives
    Vector overHeadPos = iCub->getSelfRelativePosition(a->m_ego_position);
    int driveCount = 0;
    for(map<string,Drive>::iterator drive = a->m_drives.begin(); drive != a->m_drives.end(); drive++)
    {

        double ySize = driveDimension[1] * drive->second.value + 10;
        ostringstream opcTag;
        opcTag<<a->name()<<"_"<<drive->second.name;

        Bottle cmd;
        cmd.addString("object");
        cmd.addString(opcTag.str().c_str());
        cmd.addDouble(driveDimension[0]);
        cmd.addDouble(ySize);
        cmd.addDouble(driveDimension[2]);
        cmd.addDouble(overHeadPos[0] *1000.0);
        cmd.addDouble(overHeadPos[1] *1000.0 - ySize / 2.0 + driveDimension[1] / 2.0);
        cmd.addDouble(overHeadPos[2] *1000.0 + 500 + driveCount * (driveDimension[2]+30) );
        cmd.addDouble(a->m_ego_orientation[0]);
        cmd.addDouble(a->m_ego_orientation[1]);
        cmd.addDouble(a->m_ego_orientation[2]);
        Vector color = getDriveColor(drive->second);
        cmd.addInt((int)color[0]);            // color R
        cmd.addInt((int)color[1]);            // color G
        cmd.addInt((int)color[2]);            // color B
        cmd.addDouble(1);                     // alpha coefficient [0,1]
        toGui.write(cmd);
        driveCount++;
    }
}

void GuiUpdater::resetGUI()
{
    Bottle cmd,reply;
    cmd.clear();
    reply.clear();
    cmd.addString("reset");
    toGui.write(cmd);
}

Vector GuiUpdater::getDriveColor(const Drive &d)
{
    Vector color(3);
    double x = d.value;
    double m = d.homeoStasisMin;
    double M = d.homeoStasisMax;
    if (x>=m && x<=M )
    {
        color[0] = 255 - 255 * exp(- pow((x - (M-m)/2.0),2.0)/(2*pow(M-m,2.0)));
        color[1] = 255.0;
        color[2] = 0;
    }
    else if (x<m)
    {
        color[0] = 255.0;
        color[1] = 255.0 - 255.0 * exp(- pow(x,2.0)/(2*pow(m,2.0)));
        color[2] = 0;
    }
    else if (x>M)
    {
        color[0] = 255.0;
        color[1] = 255.0 - 255.0 * exp(- pow(x - 1,2.0)/(2*pow(1-M,2.0)));
        color[2] = 0;
    }
    return color;
}
