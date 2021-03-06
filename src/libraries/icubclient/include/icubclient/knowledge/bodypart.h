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

#ifndef BODYPART_H
#define BODYPART_H

#include "object.h"

namespace icubclient{
/**
* \ingroup icubclient_representations
*
* Represents a body part of the robot
*/
class Bodypart: public Object
{
    friend class OPCClient;
public:
    Bodypart();
    Bodypart(const Bodypart &b);
    /**
    * Joint number of the represented body part
    */
    int m_joint_number;

    /**
    * Tactile number of the represented body part
    */
    int m_tactile_number;

    /**
    * The part itself, e.g. left_arm, right_arm, ...
    */
    std::string m_part;

    /**
    * The string labelling the kinect node (of a human kinematic structure, one of the ICUBCLIENT_OPC_BODY_PART_TYPE_XXX) that corresponds to the iCub's joint
    */
    std::string m_kinectNode;

    virtual bool isType(std::string _entityType) const
    {
        if (_entityType == ICUBCLIENT_OPC_ENTITY_BODYPART)
            return true;
        else
            return this->Object::isType(_entityType);
    }
    virtual yarp::os::Bottle asBottle() const;
    virtual bool             fromBottle(const yarp::os::Bottle &b);
    virtual std::string      toString() const;
};

} //namespace

#endif // BODYPART_H
