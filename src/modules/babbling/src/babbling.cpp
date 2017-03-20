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

#include "babbling.h"

#include <vector>
#include <iostream>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/video/tracking.hpp>

using namespace std;
using namespace cv;
using namespace yarp::os;
using namespace yarp::sig;

bool Babbling::configure(yarp::os::ResourceFinder &rf) {
    bool bEveryThingisGood = true;

    moduleName = rf.check("name",Value("babbling"),"module name (string)").asString();

    // part = rf.check("part",Value("left_arm")).asString();
    robot = rf.check("robot",Value("icub")).asString();
    fps = rf.check("fps",Value(30)).asInt(); //30;
    cmd_source = rf.check("cmd_source",Value("C")).asString();
    single_joint = rf.check("single_joint",Value(-1)).asInt();

    Bottle &start_pos = rf.findGroup("start_position");
    Bottle *b_start_commandHead = start_pos.find("head").asList();
    Bottle *b_start_command = start_pos.find("arm").asList();

    start_commandHead.resize(3,0.0);
    if ((b_start_commandHead->isNull()) || (b_start_commandHead->size()<3))
    {
        yWarning("Something is wrong in ini file. Default value is used");
        start_commandHead[0]=-20.0;
        start_commandHead[1]=-20.0;
        start_commandHead[2]=+10.0;
    }
    else
    {
        for(int i=0; i<b_start_commandHead->size(); i++){
            start_commandHead[i] = b_start_commandHead->get(i).asDouble();
            yDebug() << start_commandHead[i] ;
        }
    }

    if ((b_start_command->isNull()) || (b_start_command->size()<16))
    {
        yWarning("Something is wrong in ini file. Default value is used");
        start_command[0] = -45.0;
        start_command[1] = 35.0;
        start_command[2] = 0.0;
        start_command[3] = 50.0;
        start_command[4] = -45.0;
        start_command[5] = 0.0;
        start_command[6] = 0.0;
        start_command[7] = 0.0;
        start_command[8] = 10.0;
        start_command[9] = 0.0;
        start_command[10] = 0.0;
        start_command[11] = 0.0;
        start_command[12] = 0.0;
        start_command[13] = 0.0;
        start_command[14] = 0.0;
        start_command[15] = 0.0;
    }
    else
    {
        for(int i=0; i<b_start_command->size(); i++)
            start_command[i] = b_start_command->get(i).asDouble();
    }

    Bottle &babbl_par = rf.findGroup("babbling_param");
    freq = babbl_par.check("freq", Value(0.2)).asDouble();
    amp = babbl_par.check("amp", Value(5)).asDouble();
    train_duration = babbl_par.check("train_duration", Value(20.0)).asDouble();

    // keep this part if you want to use both cameras
    // if you only want to use want, delete accordingly
    if (robot=="icub")
    {
        leftCameraPort = "/"+robot+"/camcalib/left/out";
        rightCameraPort = "/"+robot+"/camcalib/right/out";
    }
    else //if (robot=='icubSim')
    {
        leftCameraPort = "/"+robot+"/cam/left";
        rightCameraPort = "/"+robot+"/cam/right";
    }

    setName(moduleName.c_str());

    // Open handler port
    if (!handlerPort.open("/" + getName() + "/rpc")) {
        cout << getName() << ": Unable to open port " << "/" << getName() << "/rpc" << endl;
        bEveryThingisGood = false;
    }

    if (!portVelocityOut.open("/" + getName() + "/velocity:o")) {
        cout << getName() << ": Unable to open port " << "/" << getName() << "/velocity:o" << endl;
        bEveryThingisGood = false;
    }

    if (!portToABM.open("/" + getName() + "/toABM")) {
        yError() << getName() << ": Unable to open port " << "/" + getName() + "/toABM";
        bEveryThingisGood = false;
    }

    if(!Network::connect(portToABM.getName(), "/autobiographicalMemory/rpc")){
        yWarning() << "Cannot connect to ABM, storing data into it will not be possible unless manual connection";
    } else {
        yInfo() << "Connected to ABM : Data are coming!";
    }


    if (!portToMatlab.open("/portToMatlab:o")) {
        cout << ": Unable to open port " << "/portToMatlab:o" << endl;
    }
    if (!portReadMatlab.open("/portReadMatlab:i")) {
        cout << ": Unable to open port " << "/portReadMatlab:i" << endl;
    }


    // Initialize iCub and Vision
    cout << "Going to initialise iCub ..." << endl;
    while (!init_iCub(part)) {
        cout << getName() << ": initialising iCub... please wait... " << endl;
        bEveryThingisGood = false;
    }

    for (int l=0; l<16; l++)
        ref_command[l] = 0;


    yDebug() << "End configuration...";

    attach(handlerPort);

    return bEveryThingisGood;
}

bool Babbling::interruptModule() {

    portVelocityOut.interrupt();
    handlerPort.interrupt();
    portToABM.interrupt();
    portToMatlab.interrupt();
    portReadMatlab.interrupt();

    yInfo() << "Bye!";

    return true;

}

bool Babbling::close() {
    cout << "Closing module, please wait ... " <<endl;

    leftArmDev.close();
    rightArmDev.close();
    headDev.close();

    portVelocityOut.interrupt();
    portVelocityOut.close();


    portToMatlab.interrupt();
    portToMatlab.close();

    portReadMatlab.interrupt();
    portReadMatlab.close();


    handlerPort.interrupt();
    handlerPort.close();

    portToABM.interrupt();
    portToABM.close();

    yInfo() << "Bye!";

    return true;
}

bool Babbling::respond(const Bottle& command, Bottle& reply) {

    string helpMessage =  string(getName().c_str()) +
            " commands are: \n " +
            "babbling arm <left/right>: motor commands sent to all the arm joints \n " +
            "babbling joint <int joint_number> <left/right>: motor commands sent to joint_number only \n " +
            "home: move the robot to home position \n " +
            "help \n " +
            "quit \n" ;

    reply.clear();

    if (command.get(0).asString()=="quit") {
        reply.addString("Quitting");
        return false;
    }
    else if (command.get(0).asString()=="help") {
        yInfo() << helpMessage;
        reply.addString(helpMessage);
    }
    else if (command.get(0).asString()=="home") {
        reply.addString("Moving to home position...");
        gotoHomePos();
        reply.addString("ack");
    }
    else if (command.get(0).asString()=="babbling") {
        if (command.get(1).asString()=="arm")
        {
            single_joint = -1;
            part_babbling = command.get(1).asString();

            if (command.get(2).asString()=="left" || command.get(2).asString()=="right")
            {
                part = command.get(2).asString() + "_arm";
                yInfo() << "Babbling "+command.get(2).asString()+" arm...";

                if (command.size()>=4) {
                    yInfo() << "Custom train_duration = " << command.get(3).asDouble() ;
                    if (command.get(3).asDouble() >= 0.0)
                    {
                        double newTrainDuration = command.get(3).asDouble();
                        double tempTrainDuration = train_duration;
                        train_duration = newTrainDuration;
                        yInfo() << "Train_Duration is changed from " << tempTrainDuration << " to " << train_duration;
                        doBabbling();
                        train_duration = tempTrainDuration;
                        yInfo() << "Going back to train_duration from config file: " << train_duration;
                        reply.addString("ack");
                        return true;
                    }
                    else
                    {
                        yError("Invalid train duration: Must be a double >= 0.0");
                        yWarning("Doing the babbling anyway with default value");
                    }
                }

                doBabbling();
                reply.addString("ack");
                return true;
            }
            else
            {
                yError("Invalid babbling part: specify LEFT or RIGHT after 'arm'.");
                reply.addString("nack");
                return false;
            }
        }
        else if (command.get(1).asString()=="joint")
        {
            single_joint = command.get(2).asInt();
            if(single_joint < 16 && single_joint>=0)
            {

                if (command.get(3).asString()=="left" || command.get(3).asString()=="right")
                {
                    part = command.get(3).asString() + "_arm";
                    yInfo() << "Babbling joint " << single_joint << " of " << part ;

                    //change train_duration if specified
                    if (command.size()>=5) {
                        yInfo() << "Custom train_duration = " << command.get(4).asDouble() ;
                        if (command.get(4).asDouble() >= 0.0)
                        {
                            double newTrainDuration = command.get(4).asDouble();
                            double tempTrainDuration = train_duration;
                            train_duration = newTrainDuration;
                            yInfo() << "Train_Duration is changed from " << tempTrainDuration << " to " << train_duration;
                            doBabbling();
                            train_duration = tempTrainDuration;
                            yInfo() << "Going back to train_duration from config file: " << train_duration;
                            reply.addString("ack");
                            return true;
                        }
                        else
                        {
                            yError("Invalid train duration: Must be a double >= 0.0");
                            yWarning("Doing the babbling anyway with default value");
                        }
                    }

                    doBabbling();
                    reply.addString("ack");
                    return true;
                }
                else
                {
                    yError("Invalid babbling part: specify LEFT or RIGHT after joint number.");
                    reply.addString("nack");
                    return false;
                }
            }
            else
            {
                yError("Invalid joint number.");
                reply.addString("nack");
                return false;
            }

        }
        else if (command.get(1).asString()=="hand")
        {
            single_joint = -1;
            part_babbling = command.get(1).asString();

            if (command.get(2).asString()=="left" || command.get(2).asString()=="right")
            {
                part = command.get(2).asString() + "_arm";
                yInfo() << "Babbling "+command.get(2).asString()+" hand...";
                doBabbling();
                reply.addString("ack");
                return true;
            }
            else
            {
                yError("Invalid babbling part: specify LEFT or RIGHT after 'arm'.");
                reply.addString("nack");
                return false;
            }


        }
        else if (command.get(1).asString()=="kinematic")
        {
            single_joint = -1;
            part_babbling = "arm";

            if (command.get(2).asString()=="left" || command.get(2).asString()=="right")
            {
                part = command.get(2).asString() + "_arm";
                yInfo() << "Babbling "+command.get(2).asString()+" arm... FOR KINEMATIC STRUCTURE";

                doBabblingKinStruct();
                reply.addString("ack");
                return true;
            }
            else
            {
                yError("Invalid babbling part: specify LEFT or RIGHT after 'arm'.");
                reply.addString("nack");
                return false;
            }
        }
        else
        {
            yInfo() << "Command not found\n" << helpMessage;
            reply.addString("nack");
            return false;
        }
    }

    return true;
}

bool Babbling::updateModule() {
    //single_joint = 1;
    //part = "right_arm";
    //doBabbling();
    return true;
}

double Babbling::getPeriod() {
    return 0.1;
}


bool Babbling::doBabbling()
{
    // First go to home position
    bool homeStart = gotoStartPos();
    if(!homeStart) {
        cout << "I got lost going home!" << endl;
    }

    yDebug() << "OK";


    for(int i=0; i<16; i++)
    {
        if(part=="right_arm"){
            ictrlRightArm->setControlMode(i,VOCAB_CM_VELOCITY);
        }
        else if(part=="left_arm"){
            ictrlLeftArm->setControlMode(i,VOCAB_CM_VELOCITY);
        }
        else
            yError() << "Don't know which part to move to do babbling." ;
    }

    if (cmd_source == "C")
    {
        double startTime = yarp::os::Time::now();

        Bottle abmCommand;
        abmCommand.addString("babbling");
        abmCommand.addString("arm");
        abmCommand.addString(part);

        yDebug() << "============> babbling with COMMAND will START" ;
        dealABM(abmCommand,1);

        while (Time::now() < startTime + train_duration){
            //            yInfo() << Time::now() << "/" << startTime + train_duration;
            double t = Time::now() - startTime;
            yInfo() << "t = " << t << "/ " << train_duration;

            babblingCommands(t,single_joint);
        }

        dealABM(abmCommand,0);

        yDebug() << "============> babbling with COMMAND is FINISHED" ;
    }
    else if(cmd_source == "M") {
        babblingCommandsMatlab();
    }

    /*yDebug() << "\n CHECKPOINT \n" ;
    Time::delay(3);

    bool homeEnd = gotoStartPos();
    if(!homeEnd) {
        cout << "I got lost going home!" << endl;
    }


    yDebug() << "\n CHECKPOINT 2 \n" ;
    Time::delay(3);*/

    return true;
}

yarp::sig::Vector Babbling::babblingCommands(double &t, int j_idx)
{

    yInfo() << "AMP " << amp<< "FREQ " << freq ;

    for (unsigned int l=0; l<command.size(); l++)
        command[l]=0;

    for (unsigned int l=0; l<16; l++)
        ref_command[l]=start_command[l] + amp*sin(freq*t * 2 * M_PI);

    yarp::sig::Vector encodersUsed;
    if(part=="right_arm" || part=="left_arm")
    {
        if(part=="right_arm")
            encodersUsed = encodersRightArm;
        else
            encodersUsed = encodersLeftArm;

        cout << j_idx << endl;

        if(j_idx != -1)
        {
            bool okEncArm = false;
            if(part=="right_arm")
                okEncArm = encsRightArm->getEncoders(encodersUsed.data());
            else
                okEncArm = encsLeftArm->getEncoders(encodersUsed.data());

            if(!okEncArm) {
                cerr << "Error receiving encoders";
                command[j_idx] = 0;
            } else {
                yDebug() << "command before correction = " << ref_command[j_idx];
                yDebug() << "current encodres : " << encodersUsed[j_idx] ;
                command[j_idx] = amp*sin(freq*t * 2 * M_PI);//1 * (ref_command[j_idx] - encodersUsed[j_idx]);
                yDebug() << "command after correction = " << command[j_idx];
                if(command[j_idx] > 50)
                    command[j_idx] = 50;
                if(command[j_idx] < -50)
                    command[j_idx] = -50;
                yDebug() << "command after saturation = " << command[j_idx];
            }
        }
        else
        {
            if(part_babbling == "arm")
            {
                bool okEncArm = false;
                if(part=="right_arm")
                    okEncArm = encsRightArm->getEncoders(encodersUsed.data());
                else
                    okEncArm = encsLeftArm->getEncoders(encodersUsed.data());
                if(!okEncArm) {
                    cerr << "Error receiving encoders";
                    for (unsigned int l=0; l<7; l++)
                        command[l] = 0;
                } else {
                    for (unsigned int l=0; l<7; l++)
                    {
                        command[l] = 10 * (ref_command[l] - encodersUsed[l]);
                        if(command[j_idx] > 20)
                            command[j_idx] = 20;
                        if(command[j_idx] < -20)
                            command[j_idx] = -20;
                    }
                }
            }
            else if(part_babbling == "hand")
            {
                bool okEncArm = false;
                if(part=="right_arm")
                    okEncArm = encsRightArm->getEncoders(encodersUsed.data());
                else
                    okEncArm = encsLeftArm->getEncoders(encodersUsed.data());
                if(!okEncArm) {
                    cerr << "Error receiving encoders";
                    for (unsigned int l=7; l<command.size(); l++)
                        command[l] = 0;
                } else {
                    for (unsigned int l=7; l<command.size(); l++)
                    {
                        command[l] = 10 * (ref_command[l] - encodersUsed[l]);
                        if(command[j_idx] > 20)
                            command[j_idx] = 20;
                        if(command[j_idx] < -20)
                            command[j_idx] = -20;
                    }
                }
            }
            else
            {
                yError("Can't babble the required body part.");
            }
        }

        Bottle& inDataB = portVelocityOut.prepare(); // Get the object
        inDataB.clear();
        for(unsigned int l=0; l<command.size(); l++)
        {
            inDataB.addDouble(command[l]);
        }
        portVelocityOut.write();

        if(part=="right_arm"){
            velRightArm->velocityMove(command.data());
        }
        else if(part=="left_arm"){
            velLeftArm->velocityMove(command.data());
        }
        else{
            yError() << "Don't know which part to move to do babbling." ;
        }

        // This delay is needed!!!
        yarp::os::Time::delay(0.05);

    }
    else
        yError() << "Which arm?";

    return command;
}


int Babbling::babblingCommandsMatlab()
{


    while(!Network::isConnected(portToMatlab.getName(), "/matlab/read")) {
        Network::connect(portToMatlab.getName(), "/matlab/read");
        cout << "Waiting for port " << portToMatlab.getName() << " to connect to " << "/matlab/read" << endl;
        Time::delay(1.0);
    }

    while(!Network::isConnected("/matlab/write", portReadMatlab.getName())) {
        Network::connect("/matlab/write", portReadMatlab.getName());
        cout << "Waiting for port " << "/matlab/write" << " to connect to " << portReadMatlab.getName() << endl;
        Time::delay(1.0);
    }

    yInfo() << "Connections ok..." ;

    for(int i=0; i<=6; i++)
    {
        ictrlLeftArm->setControlMode(i,VOCAB_CM_VELOCITY);
        ictrlRightArm->setControlMode(i,VOCAB_CM_VELOCITY);
//        ictrlLeftArm->setControlMode(i,VOCAB_CM_TORQUE);
//        ictrlRightArm->setControlMode(i,VOCAB_CM_TORQUE);
    }

    Bottle abmCommand;
    abmCommand.addString("babbling");
    abmCommand.addString("arm");
    abmCommand.addString(part);

    yDebug() << "============> babbling with COMMAND will START" ;

    /********************************** snapshot to ABM **********************************/
    dealABM(abmCommand,1);

    Bottle *endMatlab;
    Bottle *cmdMatlab;
    Bottle replyFromMatlab;
    Bottle bToMatlab;

    bool done = false;
    int nPr = 1;
    while(!done)
    {

        // Tell Matlab that motion is ready to be done
        bToMatlab.clear();replyFromMatlab.clear();
        bToMatlab.addInt(1);
        portToMatlab.write(bToMatlab,replyFromMatlab);


        for (unsigned int l=0; l<command.size(); l++)
            command[l]=0;

        //yDebug() << command.toString();

        // get the commands from Matlab
        cmdMatlab = portReadMatlab.read(true);
        if(cmdMatlab == nullptr) {
            yDebug() << "Bottle is null";
            continue;
        }

        for (int i=0; i<7; i++)
            command[i] = cmdMatlab->get(i).asDouble();


        // Move
        int j=5;
        while(j--)
        {
            if(part=="right_arm"){
                velRightArm->velocityMove(command.data());
//                for (int i=0; i<4; i++)
//                    itrqRightArm->setRefTorque(i,command[i]);
            }
            else if(part=="left_arm"){
                velLeftArm->velocityMove(command.data());
//                for (int i=0; i<4; i++)
//                {
//                    yDebug() << command[i];
//                    itrqLeftArm->setRefTorque(i,command[i]);
//                }
            }
            else
                yError() << "Don't know which part to move to do babbling." ;

            Time::delay(0.0025);// use this with iCub: 0.025
        }
//        for (unsigned int l=0; l<command.size(); l++)
//            command[l]=0;
//        if(part=="right_arm"){
//            velRightArm->velocityMove(command.data());
//        }
//        else if(part=="left_arm"){
//            velLeftArm->velocityMove(command.data());
//        }
//        else
//            yError() << "Don't know which part to move to do babbling." ;

        // Tell Matlab that motion is done
        bToMatlab.clear();replyFromMatlab.clear();
        bToMatlab.addInt(1);
        portToMatlab.write(bToMatlab,replyFromMatlab);

        endMatlab = portReadMatlab.read(true) ;
        done = endMatlab->get(0).asInt();

        nPr = nPr +1;
    }

    for (unsigned int l=0; l<command.size(); l++)
        command[l]=0;
    if(part=="right_arm"){
        velRightArm->velocityMove(command.data());
    }
    else if(part=="left_arm"){
        velLeftArm->velocityMove(command.data());
    }
    else
        yError() << "Don't know which part to move to do babbling." ;

    yInfo() << "Going to tell Matlab that ports will be closed here.";
    // Tell Matlab that ports will be closed here
    bToMatlab.clear();replyFromMatlab.clear();
    bToMatlab.addInt(1);
    portToMatlab.write(bToMatlab,replyFromMatlab);


    yInfo() << "Finished and ports to/from Matlab closed.";
    yDebug() << "============> babbling is FINISHED";

    dealABM(abmCommand,0);

    return 0;
}


bool Babbling::moveHeadToStartPos()
{
    yarp::sig::Vector ang=start_commandHead;
    if (part=="right_arm")
        ang[0]=-ang[0];
    return igaze->lookAtAbsAngles(ang);
}


bool Babbling::doBabblingKinStruct()
{
    // First go to start position
    igaze->stopControl();
    velLeftArm->stop();
    velRightArm->stop();

    yarp::os::Time::delay(1.0);

    moveHeadToStartPos();

    /* Move arm to start position */
    yarp::sig::Vector cmd_arms;
    cmd_arms.resize(16, 0.0);
    cmd_arms[0]= -70;
    cmd_arms[1]= 40;
    cmd_arms[2]= 60;
    cmd_arms[3]= 30;
    cmd_arms[4]= 10;
    cmd_arms[5]= -30;
    cmd_arms[6]= 0;
    cmd_arms[7]= 60;

    cout << cmd_arms[0]<<" " << cmd_arms[1] <<" " << cmd_arms[7] <<endl;

    if(part=="left_arm" || part=="right_arm")
    {
        command = cmd_arms;

        if(part=="left_arm")
        {
            for(int i=0; i<16; i++)
                ictrlLeftArm->setControlMode(i, VOCAB_CM_POSITION);
            posLeftArm->positionMove(command.data());
        }
        else
        {
            for(int i=0; i<16; i++)
                ictrlRightArm->setControlMode(i, VOCAB_CM_POSITION);
            posRightArm->positionMove(command.data());
        }

        bool done_head=false;
        bool done_arm=false;
        while (!done_head || !done_arm) {
            yInfo() << "Wait for position moves to finish" ;
            igaze->checkMotionDone(&done_head);
            if(part=="left_arm") {
                done_arm = true;
                for(int i = 0; i< 8; i++) {
                    bool done_joint=false;
                    posLeftArm->checkMotionDone(i, &done_joint);
                    if(!done_joint) {
                        done_arm = false;
                        break;
                    }
                }
            }
            else
                posRightArm->checkMotionDone(&done_arm);
            Time::delay(0.04);
        }
        yInfo() << "Done." ;

        Time::delay(1.0);
    }
    else
        yError() << "Don't know which part to move to start position." ;



    for(int i=0; i<8; i++)
    {
        if(part=="right_arm"){
            ictrlRightArm->setControlMode(i,VOCAB_CM_VELOCITY);
        }
        else if(part=="left_arm"){
            ictrlLeftArm->setControlMode(i,VOCAB_CM_VELOCITY);
        }
        else
            yError() << "Don't know which part to move to do babbling." ;


    }


    Bottle abmCommand;
    abmCommand.addString("babbling");
    abmCommand.addString("arm");

    dealABM(abmCommand,1);

    yInfo() << "AMP " << amp<< "FREQ " << freq ;

    double startTime = yarp::os::Time::now();
    while (Time::now() < startTime + train_duration){
        //            yInfo() << Time::now() << "/" << startTime + train_duration;
        double t = Time::now() - startTime;
        yInfo() << "t = " << t << "/ " << train_duration;

        /////////////////////////////////////////////////////////////////////////
        for (unsigned int l=0; l<8; l++)
            command[l]=0;


        for (unsigned int l=0; l<8; l++)
            ref_command[l]=cmd_arms[l] + amp*sin(freq*t * 2 * M_PI);//0;
        ref_command[1]=cmd_arms[1] + 10*sin(0.1*t * 2 * M_PI);
        ref_command[3]=cmd_arms[3] + 5*sin(0.35*t * 2 * M_PI);
        ref_command[6]=cmd_arms[6] + 35*sin(0.8*t * 2 * M_PI);
        cout <<"___________________________"<< endl;
        cout <<"Ref "<< ref_command[1] <<" " << ref_command[3] <<" "<< ref_command[6] <<" "<< endl;

        yarp::sig::Vector encodersUsed;
        if(part=="right_arm" || part=="left_arm")
        {

            if(part=="right_arm")
            {
                while (!encsRightArm->getEncoders(encodersRightArm.data()))
                {
                    Time::delay(0.1);
                    yInfo() << "Wait for arm encoders";
                }
                encodersUsed = encodersRightArm;
            }
            else
            {
                while (!encsLeftArm->getEncoders(encodersLeftArm.data()))
                {
                    Time::delay(0.1);
                    yInfo() << "Wait for arm encoders";
                }
                encodersUsed = encodersLeftArm;
            }

            if(part_babbling == "arm")
            {
                bool okEncArm = false;
                if(part=="right_arm")
                    okEncArm = encsRightArm->getEncoders(encodersUsed.data());
                else
                    okEncArm = encsLeftArm->getEncoders(encodersUsed.data());
                if(!okEncArm) {
                    cerr << "Error receiving encoders";
                    for (unsigned int l=0; l<8; l++)
                        command[l] = 0;
                } else {
                    for (unsigned int l=0; l<8; l++)
                    {
                        command[l] = 0 * (ref_command[l] - encodersUsed[l]);
                    }
                    command[1] = 10 * (ref_command[1] - encodersUsed[1]);
                    command[3] = 10 * (ref_command[3] - encodersUsed[3]);
                    command[6] = 10 * (ref_command[6] - encodersUsed[6]);
                    for (unsigned int l=0; l<8; l++)
                    {
                        if(command[l] > 20)
                            command[l] = 20;
                        if(command[l] < -20)
                            command[l] = -20;
                    }
                    cout <<"---------------------------"<< endl;
                    cout <<"used " << encodersUsed[1] <<" " << encodersUsed[3] <<" "<< encodersUsed[6] <<" "<< endl;
                }
            }
            else
            {
                yError("Can't babble the required body part.");
            }


            Bottle& inDataB = portVelocityOut.prepare(); // Get the object
            inDataB.clear();
            for(unsigned int l=0; l<8; l++)
            {
                inDataB.addDouble(command[l]);
            }
            portVelocityOut.write();

            cout <<"---------------------------"<< endl;
            cout << command[1] <<" " << command[3] <<" "<< command[6] <<" "<< endl;
            cout <<"___________________________"<< endl;

            if(part=="right_arm"){
                velRightArm->velocityMove(command.data());
            }
            else if(part=="left_arm"){
                velLeftArm->velocityMove(command.data());
            }
            else{
                yError() << "Don't know which part to move to do babbling." ;
            }

            // This delay is needed!!!
            yarp::os::Time::delay(0.05);

        }
        else
            yError() << "Which arm?";
        ////////////////////////////////////////////////////////////////////////////////
    }

    dealABM(abmCommand,0);

    bool homeEnd = gotoStartPos();
    if(!homeEnd) {
        cout << "I got lost going home!" << endl;
    }
    cout <<"___________________________"<< endl;
    yDebug() << "Finished babbling for kinematic structure";
    cout <<"___________________________"<< endl;

    return true;
}


bool Babbling::gotoStartPos()
{
    igaze->stopControl();
    velLeftArm->stop();
    velRightArm->stop();

    yarp::os::Time::delay(2.0);

    moveHeadToStartPos();

    /* Move arm to start position */
    if(part=="left_arm" || part=="right_arm")
    {
        if(part=="left_arm")
        {
            command = encodersLeftArm;
            for(int i=0; i<16; i++){
                ictrlLeftArm->setControlMode(i, VOCAB_CM_POSITION);
                command[i] = start_command[i];}
            posLeftArm->positionMove(command.data());
        }
        else
        {
            command = encodersRightArm;
            for(int i=0; i<16; i++){
                ictrlRightArm->setControlMode(i, VOCAB_CM_POSITION);
                command[i] = start_command[i];}
            posRightArm->positionMove(command.data());
        }

        bool done_head=false;
        bool done_arm=false;
        while (!done_head || !done_arm) {
            yInfo() << "Wait for position moves to finish" ;
            igaze->checkMotionDone(&done_head);
            if(part=="left_arm") {
                done_arm = true;
                for(int i = 0; i< 8; i++) {
                    bool done_joint=false;
                    posLeftArm->checkMotionDone(i, &done_joint);
                    if(!done_joint) {
                        done_arm = false;
                        break;
                    }
                }
            }
            else
                posRightArm->checkMotionDone(&done_arm);
            Time::delay(0.04);
        }
        yInfo() << "Done." ;

        Time::delay(1.0);
    }
    else
        yError() << "Don't know which part to move to start position." ;


    return true;
}

bool Babbling::gotoHomePos()
{
    igaze->stopControl();
    velLeftArm->stop();
    velRightArm->stop();

    yarp::os::Time::delay(2.0);

    /* Move head to home position */
    yarp::sig::Vector ang(3,0.0);
    igaze->lookAtAbsAngles(ang);
    igaze->waitMotionDone();
    yInfo() << "Done head.";

    /* Move arms to home position */
    for(int i=0; i<16; i++)
    {
        ictrlRightArm->setControlMode(i, VOCAB_CM_POSITION);
        ictrlLeftArm->setControlMode(i, VOCAB_CM_POSITION);
    }
    command[0] = -25;   command[1] = 20;    command[2] = 0;     command[3] = 50;
    command[4] = 0;     command[5] = 0;     command[6] = 0;     command[7] = 60;
    command[8] = 20;    command[9] = 20;    command[10] = 20;   command[11] = 10;
    command[12] = 10;   command[13] = 10;   command[14] = 10;   command[15] = 10;

    posLeftArm->positionMove(command.data());
    bool done_arm_l=false;
    while (!done_arm_l) {
        yInfo() << "Wait for left arm position moves to finish" ;
        posLeftArm->checkMotionDone(&done_arm_l);
        Time::delay(0.04);
    }
    yInfo() << "Done left arm." ;

    posRightArm->positionMove(command.data());
    bool done_arm_r=false;
    while (!done_arm_r) {
        yInfo() << "Wait for right arm position moves to finish" ;
        posRightArm->checkMotionDone(&done_arm_r);
        Time::delay(0.04);
    }
    yInfo() << "Done right arm." ;

    Time::delay(1.0);

    return true;
}


bool Babbling::init_iCub(string &part)
{
    /* Create PolyDriver for left arm */
    Property option;

    string portnameLeftArm = "left_arm";//part;
    cout << part << endl;
    option.put("robot", robot.c_str());
    option.put("device", "remote_controlboard");
    Value& robotnameLeftArm = option.find("robot");

    string sA("/babbling/");
    sA += robotnameLeftArm.asString();
    sA += "/";
    sA += portnameLeftArm.c_str();
    sA += "/control";
    option.put("local", sA.c_str());

    sA.clear();
    sA += "/";
    sA += robotnameLeftArm.asString();
    sA += "/";
    sA += portnameLeftArm.c_str();
    cout << sA << endl;
    option.put("remote", sA.c_str());

    yDebug() << option.toString().c_str() ;

    if (!leftArmDev.open(option)) {
        yError() << "Device not available.  Here are the known devices:";
        yError() << yarp::dev::Drivers::factory().toString();
        return false;
    }

    leftArmDev.view(posLeftArm);
    leftArmDev.view(velLeftArm);
    leftArmDev.view(itrqLeftArm);
    leftArmDev.view(encsLeftArm);
    leftArmDev.view(ictrlLeftArm);
    leftArmDev.view(ictrlLimLeftArm);

    double minLimArm[16];
    double maxLimArm[16];
    for (int l=0; l<16; l++)
        ictrlLimLeftArm->getLimits(l,&minLimArm[l],&maxLimArm[l]);
    //    for (int l=7; l<16; l++)
    //        start_command[l] = (maxLimArm[l]-minLimArm[l])/2;
    for (int l=0; l<16; l++)
        yInfo() << "Joint " << l << ": limits = [" << minLimArm[l] << "," << maxLimArm[l] << "]. start_commad = " << start_command[l];

    if (posLeftArm==NULL || encsLeftArm==NULL || velLeftArm==NULL || itrqLeftArm==NULL || ictrlLeftArm==NULL ){
        cout << "Cannot get interface to robot device" << endl;
        leftArmDev.close();
    }

    if (encsLeftArm==NULL){
        yError() << "Cannot get interface to robot device";
        leftArmDev.close();
    }

    int nj = 0;
    posLeftArm->getAxes(&nj);
    velLeftArm->getAxes(&nj);
    itrqLeftArm->getAxes(&nj);
    encsLeftArm->getAxes(&nj);
    encodersLeftArm.resize(nj);

    yInfo() << "Wait for arm encoders";
    while (!encsLeftArm->getEncoders(encodersLeftArm.data()))
    {
        Time::delay(0.1);
        yInfo() << "Wait for arm encoders";
    }

    /* Create PolyDriver for right arm */
    string portnameRightArm = "right_arm";//part;
    cout << part << endl;
    option.put("robot", robot.c_str());
    option.put("device", "remote_controlboard");
    Value& robotnameRightArm = option.find("robot");

    string sAr("/babbling/");
    sAr += robotnameRightArm.asString();
    sAr += "/";
    sAr += portnameRightArm.c_str();
    sAr += "/control";
    option.put("local", sAr.c_str());

    sAr.clear();
    sAr += "/";
    sAr += robotnameRightArm.asString();
    sAr += "/";
    sAr += portnameRightArm.c_str();
    cout << sAr << endl;
    option.put("remote", sAr.c_str());

    yDebug() << option.toString().c_str();

    if (!rightArmDev.open(option)) {
        yError() << "Device not available.  Here are the known devices:";
        yError() << yarp::dev::Drivers::factory().toString();
        return false;
    }

    rightArmDev.view(posRightArm);
    rightArmDev.view(velRightArm);
    rightArmDev.view(itrqRightArm);
    rightArmDev.view(encsRightArm);
    rightArmDev.view(ictrlRightArm);
    rightArmDev.view(ictrlLimRightArm);

    for (int l=0; l<16; l++)
        ictrlLimRightArm->getLimits(l,&minLimArm[l],&maxLimArm[l]);
    //    for (int l=7; l<16; l++)
    //        start_command[l] = (maxLimArm[l]-minLimArm[l])/2;
    for (int l=0; l<16; l++)
        yInfo() << "Joint " << l << ": limits = [" << minLimArm[l] << "," << maxLimArm[l] << "]. start_commad = " << start_command[l];


    if (posRightArm==NULL || encsRightArm==NULL || velRightArm==NULL || itrqRightArm==NULL  || ictrlRightArm==NULL ){
        cout << "Cannot get interface to robot device" << endl;
        rightArmDev.close();
    }

    if (encsRightArm==NULL){
        yError() << "Cannot get interface to robot device";
        rightArmDev.close();
    }

    nj = 0;
    posRightArm->getAxes(&nj);
    velRightArm->getAxes(&nj);
    itrqRightArm->getAxes(&nj);
    encsRightArm->getAxes(&nj);
    encodersRightArm.resize(nj);

    yInfo() << "Wait for arm encoders";
    while (!encsRightArm->getEncoders(encodersRightArm.data()))
    {
        Time::delay(0.1);
        yInfo() << "Wait for arm encoders";
    }

    yInfo() << "Arms initialized.";

    /* Init. head */
    option.clear();
    option.put("device", "gazecontrollerclient");
    option.put("remote", "/iKinGazeCtrl");
    option.put("local",  "/babbling/gaze");

    if (!headDev.open(option)) {
        yError() << "Device not available.  Here are the known devices:";
        yError() << yarp::dev::Drivers::factory().toString();
        return false;
    }

    headDev.view(igaze);

    yInfo() << "Head initialized.";

//    /* Set velocity control for arm */
//    yInfo() << "Set velocity control mode";
//    for(int i=0; i<16; i++)
//    {
////        ictrlLeftArm->setControlMode(i,VOCAB_CM_VELOCITY);
////        ictrlRightArm->setControlMode(i,VOCAB_CM_VELOCITY);
//        ictrlLeftArm->setControlMode(i,VOCAB_CM_TORQUE);
//        ictrlRightArm->setControlMode(i,VOCAB_CM_TORQUE);
//    }

    yInfo() << "> Initialisation done.";

    return true;
}


bool Babbling::dealABM(const Bottle& command, bool begin)
{
    yDebug() << "Dealing with ABM: bottle received = " << command.toString() << " of size = " << command.size() << " begin: " << begin;

    Bottle bABM, bABMreply;
    bABM.addString("snapshot");
    Bottle bSubMain;
    bSubMain.addString("action");
    bSubMain.addString(command.get(0).asString());
    bSubMain.addString("action");
    Bottle bSubArgument;
    bSubArgument.addString("arguments");
    Bottle bSubSubArgument;
    bSubSubArgument.addString(command.get(1).toString());
    bSubSubArgument.addString("limb");
    Bottle bSubSubArgument2;
    bSubSubArgument2.addString(part);
    bSubSubArgument2.addString("side");
    Bottle bSubSubArgument3;
    bSubSubArgument3.addString(robot);
    bSubSubArgument3.addString("agent1");
    if(command.size()>=3)
    {
        Bottle bSubSubArgument4;
        bSubSubArgument4.addString(command.get(2).toString());
        bSubSubArgument4.addString("joint");
        bSubArgument.addList() = bSubSubArgument4;
    }
    Bottle bSubSubArgument5;
    bSubSubArgument5.addString(to_string(freq));
    bSubSubArgument5.addString("freq");
    Bottle bSubSubArgument6;
    bSubSubArgument6.addString(to_string(amp));
    bSubSubArgument6.addString("amp");
    Bottle bSubSubArgument7;
    bSubSubArgument7.addString(to_string(train_duration));
    bSubSubArgument7.addString("train_duration");

    Bottle bBegin;
    bBegin.addString("begin");
    bBegin.addInt(begin);

    bABM.addList() = bSubMain;
    bSubArgument.addList() = bSubSubArgument;
    bSubArgument.addList() = bSubSubArgument2;
    bSubArgument.addList() = bSubSubArgument3;
    //4 optional is already done if needed
    bSubArgument.addList() = bSubSubArgument5;
    bSubArgument.addList() = bSubSubArgument6;
    bSubArgument.addList() = bSubSubArgument7;

    bABM.addList() = bSubArgument;
    bABM.addList() = bBegin;
    yInfo() << "Bottle to ABM: " << bABM.toString();

    if(!Network::isConnected(portToABM.getName(), "/autobiographicalMemory/rpc")) {
        Network::connect(portToABM.getName(), "/autobiographicalMemory/rpc");
    }
    if(Network::isConnected(portToABM.getName(), "/autobiographicalMemory/rpc")) {
        portToABM.write(bABM,bABMreply);
    }

    yDebug() << "Finished dealing with ABM";

    if (bABMreply.isNull()) {
        yWarning() << "Reply from ABM is null : NOT connected?";
        return false;
    } else if (bABMreply.get(0).asString()=="nack"){
        yWarning() << "Got nack from ABM: " << bABMreply.toString();
        return false;
    } else {
        return true;
    }
}
