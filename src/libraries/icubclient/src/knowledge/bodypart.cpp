/*
 * Copyright (C) 2015 WYSIWYD Consortium, European Commission FP7 Project ICT-612139
 * Authors: Tobias Fischer
 * email:   t.fischer@imperial.ac.uk
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

#include "icubclient/knowledge/bodypart.h"

using namespace std;
using namespace yarp::os;
using namespace yarp::sig;
using namespace yarp::dev;
using namespace icubclient;

Bodypart::Bodypart():Object()
{
    m_entity_type = ICUBCLIENT_OPC_ENTITY_BODYPART;
    m_joint_number = -1;
    m_tactile_number = -1;
    m_part = "unknown";
    m_present = 1.0;
    m_kinectNode = "unknown";
}


Bodypart::Bodypart(const Bodypart &b):Object(b)
{
    this->m_entity_type = b.m_entity_type;
    this->m_joint_number = b.m_joint_number;
    this->m_tactile_number = b.m_tactile_number;
    this->m_part = b.m_part;
    this->m_present = 1.0;
    this->m_kinectNode = b.m_kinectNode;
}

Bottle Bodypart::asBottle() const
{
    //Get the Object bottle
    Bottle b = this->Object::asBottle();

    Bottle bSub;
    bSub.addString("joint_number");
    bSub.addInt(m_joint_number);
    b.addList() = bSub;
    bSub.clear();
    bSub.addString("tactile_number");
    bSub.addInt(m_tactile_number);
    b.addList() = bSub;
    bSub.clear();
    bSub.addString("part");
    bSub.addString(m_part);
    b.addList() = bSub;
    bSub.clear();
    bSub.addString("kinectNode");
    bSub.addString(m_kinectNode);
    b.addList() = bSub;
    bSub.clear();

    return b;
}

string Bodypart::toString() const
{
    std::ostringstream oss;
    oss<< this->Object::toString();
    oss<< "joint number: \t\t";
    oss<< m_joint_number << endl;
    oss<< "tactile number: \t\t";
    oss<< m_tactile_number << endl;
    oss<< "part: \t\t";
    oss<< m_part <<endl;
    oss<< "kinectNode: \t\t";
    oss<< m_kinectNode <<endl;
    return oss.str();
}

bool Bodypart::fromBottle(const Bottle &b)
{
    if (!this->Object::fromBottle(b))
        return false;

    if (!b.check("joint_number") ||
        !b.check("tactile_number") ||
        !b.check("part") ||
        !b.check("kinectNode"))
    {
        return false;
    }

    m_joint_number       = b.find("joint_number").asInt();
    m_tactile_number     = b.find("tactile_number").asInt();
    m_part               = b.find("part").asString();
    m_kinectNode         = b.find("kinectNode").asString();

    return true;
}
