/* 
 * Copyright (C) 2014 WYSIWYD Consortium, European Commission FP7 Project ICT-612139
 * Authors: Martina Zambelli
 * email:   m.zambelli13@imperial.ac.uk
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

#ifndef _BABBLING_H_
#define _BABBLING_H_

#include <opencv2/opencv.hpp>

#include <yarp/sig/all.h>
#include <yarp/os/all.h>
#include <yarp/dev/all.h>

#include <wrdac/clients/icubClient.h>

class Babbling : public yarp::os::RFModule {
private:
    std::string moduleName;

    yarp::os::Port handlerPort;

    yarp::os::BufferedPort<yarp::os::Bottle> portVelocityOut;
    yarp::os::Port portToMatlab;
    yarp::os::BufferedPort<yarp::os::Bottle> portReadMatlab;

    yarp::os::RpcClient portToABM;

    yarp::dev::IPositionControl* posLeftArm;
    yarp::dev::IVelocityControl* velLeftArm;
    yarp::dev::ITorqueControl *itrqLeftArm;
    yarp::dev::IEncoders* encsLeftArm;
    yarp::dev::IControlMode2 *ictrlLeftArm;
    yarp::dev::IControlLimits *ictrlLimLeftArm;

    yarp::dev::IPositionControl* posRightArm;
    yarp::dev::IVelocityControl* velRightArm;
    yarp::dev::ITorqueControl *itrqRightArm;
    yarp::dev::IEncoders* encsRightArm;
    yarp::dev::IControlMode2 *ictrlRightArm;
    yarp::dev::IControlLimits *ictrlLimRightArm;

    yarp::dev::IGazeControl* igaze;

    yarp::dev::PolyDriver leftArmDev;
    yarp::dev::PolyDriver rightArmDev;
    yarp::dev::PolyDriver headDev;

    yarp::sig::Vector encodersLeftArm, encodersRightArm, cmd, command, new_command, tmpSpeed, tmpAcc;
    yarp::sig::Vector handTarget, armTarget, fingerTarget;    

    std::string part;
    std::string robot;
    std::string arm;
    std::string cmd_source;
    int single_joint;
    int fps;
    std::string part_babbling;

    std::string leftCameraPort, rightCameraPort;

    double freq, amp;
    double train_duration;
    
    double start_command[16];
    double ref_command[16];
    yarp::sig::Vector start_commandHead;

public:
    bool configure(yarp::os::ResourceFinder &rf);
    bool interruptModule();
    bool close();
    bool respond(const yarp::os::Bottle& command, yarp::os::Bottle& reply);
    double getPeriod();
    bool updateModule();

private:
    bool init_iCub(std::string &part);
    bool doBabbling();
    yarp::sig::Vector babblingCommands(double &t, int j_idx);
    int babblingCommandsMatlab();
    bool doBabblingKinStruct();
    bool moveHeadToStartPos();
    bool gotoStartPos();
    bool gotoHomePos();
    bool dealABM(const yarp::os::Bottle& command, bool begin);
};

#endif // _BABBLING_H_

