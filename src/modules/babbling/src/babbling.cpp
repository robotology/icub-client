/*
 * Copyright (C) 2014 WYSIWYD Consortium, European Commission FP7 Project
 * ICT-612139
 * Authors: Martina Zambelli
 * email:   m.zambelli13@imperial.ac.uk
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

#include "babbling.h"

#include <iostream>

using namespace std;
using namespace yarp::os;
using namespace yarp::sig;

bool Babbling::configure(yarp::os::ResourceFinder &rf) {
    bool bEveryThingisGood = true;

    string moduleName =
            rf.check("name", Value("babbling"), "module name (string)").asString();

    // part = rf.check("part",Value("left_arm")).asString();
    robot = rf.check("robot", Value("icub")).asString();
    single_joint = rf.check("single_joint", Value(-1)).asInt();

    Bottle &start_pos = rf.findGroup("start_position");
    Bottle *b_start_commandHead = start_pos.find("head").asList();
    Bottle *b_start_command = start_pos.find("arm").asList();

    start_commandHead.resize(3, 0.0);
    if ((b_start_commandHead->isNull()) || (b_start_commandHead->size() < 3)) {
        yWarning("Something is wrong in ini file. Default value is used");
        start_commandHead[0] = -20.0;
        start_commandHead[1] = -20.0;
        start_commandHead[2] = +10.0;
    } else {
        for (int i = 0; i < b_start_commandHead->size(); i++) {
            start_commandHead[i] = b_start_commandHead->get(i).asDouble();
            yDebug() << start_commandHead[i];
        }
    }

    if ((b_start_command->isNull()) || (b_start_command->size() < 16)) {
        yWarning("Something is wrong in ini file. Default values are used");
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
    } else {
        for (int i = 0; i < b_start_command->size(); i++)
            start_command[i] = b_start_command->get(i).asDouble();
    }

    Bottle &babbl_par = rf.findGroup("babbling_param");
    freq = babbl_par.check("freq", Value(0.2)).asDouble();
    amp = babbl_par.check("amp", Value(5)).asDouble();
    train_duration = babbl_par.check("train_duration", Value(20.0)).asDouble();

    setName(moduleName.c_str());

    // Open handler port
    if (!handlerPort.open("/" + getName() + "/rpc")) {
        yError() << getName() << ": Unable to open port "
                 << "/" << getName() << "/rpc";
        bEveryThingisGood = false;
    }

    if (!portToABM.open("/" + getName() + "/toABM")) {
        yError() << getName() << ": Unable to open port "
                 << "/" + getName() + "/toABM";
        bEveryThingisGood = false;
    }

    if (!Network::connect(portToABM.getName(), "/autobiographicalMemory/rpc")) {
        yWarning() << "Cannot connect to ABM, storing data into it will not be "
                      "possible unless manual connection";
    } else {
        yInfo() << "Connected to ABM : Data are coming!";
    }

    // Initialize iCub and Vision
    yInfo() << "Going to initialise iCub ...";
    while (!init_iCub(part)) {
        yDebug() << getName() << ": initialising iCub... please wait... ";
    }

    for (int l = 0; l < 16; l++) {
        ref_command[l] = 0;
    }

    yDebug() << "End configuration...";

    attach(handlerPort);

    return bEveryThingisGood;
}

bool Babbling::interruptModule() {
    handlerPort.interrupt();
    portToABM.interrupt();

    yInfo() << "Bye!";

    return true;
}

bool Babbling::close() {
    yInfo() << "Closing module, please wait ... ";

    leftArmDev.close();
    rightArmDev.close();
    headDev.close();

    handlerPort.interrupt();
    handlerPort.close();

    portToABM.interrupt();
    portToABM.close();

    yInfo() << "Bye!";

    return true;
}

bool Babbling::respond(const Bottle &command, Bottle &reply) {
    string helpMessage = string(getName().c_str()) + " commands are: \n " +
            "babbling arm <left/right>: motor commands sent to all "
            "the arm joints \n " +
            "babbling joint <int joint_number> <left/right>: motor "
            "commands sent to joint_number only \n " +
            "help \n " + "quit \n";

    reply.clear();

    if (command.get(0).asString() == "quit") {
        reply.addString("Quitting");
        return false;
    } else if (command.get(0).asString() == "help") {
        yInfo() << helpMessage;
        reply.addString(helpMessage);
    } else if (command.get(0).asString() == "babbling") {
        if (command.get(1).asString() == "arm") {
            single_joint = -1;
            part_babbling = command.get(1).asString();

            if (command.get(2).asString() == "left" ||
                    command.get(2).asString() == "right") {
                part = command.get(2).asString() + "_arm";
                yInfo() << "Babbling " + command.get(2).asString() + " arm...";

                if (command.size() >= 4) {
                    yInfo() << "Custom train_duration = " << command.get(3).asDouble();
                    if (command.get(3).asDouble() >= 0.0) {
                        double newTrainDuration = command.get(3).asDouble();
                        double tempTrainDuration = train_duration;
                        train_duration = newTrainDuration;
                        yInfo() << "Train_Duration is changed from " << tempTrainDuration
                                << " to " << train_duration;
                        doBabbling();
                        train_duration = tempTrainDuration;
                        yInfo() << "Going back to train_duration from config file: "
                                << train_duration;
                        reply.addString("ack");
                        return true;
                    } else {
                        yError("Invalid train duration: Must be a double >= 0.0");
                        yWarning("Doing the babbling anyway with default value");
                    }
                }

                doBabbling();
                reply.addString("ack");
                return true;
            } else {
                yError("Invalid babbling part: specify LEFT or RIGHT after 'arm'.");
                reply.addString("nack");
                return false;
            }
        } else if (command.get(1).asString() == "joint") {
            single_joint = command.get(2).asInt();
            if (single_joint < 16 && single_joint >= 0) {
                if (command.get(3).asString() == "left" ||
                        command.get(3).asString() == "right") {
                    part = command.get(3).asString() + "_arm";
                    yInfo() << "Babbling joint " << single_joint << " of " << part;

                    // change train_duration if specified
                    if (command.size() >= 5) {
                        yInfo() << "Custom train_duration = " << command.get(4).asDouble();
                        if (command.get(4).asDouble() >= 0.0) {
                            double newTrainDuration = command.get(4).asDouble();
                            double tempTrainDuration = train_duration;
                            train_duration = newTrainDuration;
                            yInfo() << "Train_Duration is changed from " << tempTrainDuration
                                    << " to " << train_duration;
                            doBabbling();
                            train_duration = tempTrainDuration;
                            yInfo() << "Going back to train_duration from config file: "
                                    << train_duration;
                            reply.addString("ack");
                            return true;
                        } else {
                            yError("Invalid train duration: Must be a double >= 0.0");
                            yWarning("Doing the babbling anyway with default value");
                        }
                    }

                    doBabbling();
                    reply.addString("ack");
                    return true;
                } else {
                    yError(
                                "Invalid babbling part: specify LEFT or RIGHT after joint "
                                "number.");
                    reply.addString("nack");
                    return false;
                }
            } else {
                yError("Invalid joint number.");
                reply.addString("nack");
                return false;
            }
        } else if (command.get(1).asString() == "hand") {
            single_joint = -1;
            part_babbling = command.get(1).asString();

            if (command.get(2).asString() == "left" ||
                    command.get(2).asString() == "right") {
                part = command.get(2).asString() + "_arm";
                yInfo() << "Babbling " + command.get(2).asString() + " hand...";
                doBabbling();
                reply.addString("ack");
                return true;
            } else {
                yError("Invalid babbling part: specify LEFT or RIGHT after 'arm'.");
                reply.addString("nack");
                return false;
            }
        } else {
            yInfo() << "Command not found\n" << helpMessage;
            reply.addString("nack");
            return false;
        }
    }

    return true;
}

bool Babbling::updateModule() {
    return true;
}

double Babbling::getPeriod() {
    return 0.1;
}

bool Babbling::doBabbling() {
    // First go to home position
    bool homeStart = gotoStartPos();
    if (!homeStart) {
        yError() << "I got lost going home!";
    }

    yDebug() << "OK";

    for (int i = 0; i < 16; i++) {
        if (part == "right_arm") {
            ictrlRightArm->setControlMode(i, VOCAB_CM_VELOCITY);
        } else if (part == "left_arm") {
            ictrlLeftArm->setControlMode(i, VOCAB_CM_VELOCITY);
        } else {
            yError() << "Don't know which part to move to do babbling.";
        }
    }

    double startTime = yarp::os::Time::now();

    Bottle abmCommand;
    abmCommand.addString("babbling");
    abmCommand.addString("arm");
    abmCommand.addString(part);

    yDebug() << "============> babbling with COMMAND will START";
    dealABM(abmCommand, 1);

    while (Time::now() < startTime + train_duration) {
        double t = Time::now() - startTime;
        yInfo() << "t = " << t << "/ " << train_duration;

        babblingCommands(t, single_joint);
    }

    dealABM(abmCommand, 0);

    yDebug() << "============> babbling with COMMAND is FINISHED";

    return true;
}

yarp::sig::Vector Babbling::babblingCommands(double &t, int j_idx) {
    yInfo() << "AMP " << amp << "FREQ " << freq;

    for (unsigned int l = 0; l < command.size(); l++) command[l] = 0;

    for (unsigned int l = 0; l < 16; l++)
        ref_command[l] = start_command[l] + amp * sin(freq * t * 2 * M_PI);

    yarp::sig::Vector encodersUsed;
    if (part == "right_arm" || part == "left_arm") {
        if (part == "right_arm")
            encodersUsed = encodersRightArm;
        else
            encodersUsed = encodersLeftArm;

        yDebug() << "j_idx: " << j_idx;

        if (j_idx != -1) {
            bool okEncArm = false;
            if (part == "right_arm")
                okEncArm = encsRightArm->getEncoders(encodersUsed.data());
            else
                okEncArm = encsLeftArm->getEncoders(encodersUsed.data());

            if (!okEncArm) {
                yError() << "Error receiving encoders";
                command[j_idx] = 0;
            } else {
                yDebug() << "command before correction = " << ref_command[j_idx];
                yDebug() << "current encoders : " << encodersUsed[j_idx];
                command[j_idx] =
                        amp * sin(freq * t * 2 *
                                  M_PI);  // 1 * (ref_command[j_idx] - encodersUsed[j_idx]);
                yDebug() << "command after correction = " << command[j_idx];
                if (command[j_idx] > 50) command[j_idx] = 50;
                if (command[j_idx] < -50) command[j_idx] = -50;
                yDebug() << "command after saturation = " << command[j_idx];
            }
        } else {
            if (part_babbling == "arm") {
                bool okEncArm = false;
                if (part == "right_arm")
                    okEncArm = encsRightArm->getEncoders(encodersUsed.data());
                else
                    okEncArm = encsLeftArm->getEncoders(encodersUsed.data());
                if (!okEncArm) {
                    yError() << "Error receiving encoders";
                    for (unsigned int l = 0; l < 7; l++) command[l] = 0;
                } else {
                    for (unsigned int l = 0; l < 7; l++) {
                        command[l] = 10 * (ref_command[l] - encodersUsed[l]);
                        if (command[j_idx] > 20) command[j_idx] = 20;
                        if (command[j_idx] < -20) command[j_idx] = -20;
                    }
                }
            } else if (part_babbling == "hand") {
                bool okEncArm = false;
                if (part == "right_arm")
                    okEncArm = encsRightArm->getEncoders(encodersUsed.data());
                else
                    okEncArm = encsLeftArm->getEncoders(encodersUsed.data());
                if (!okEncArm) {
                    yError() << "Error receiving encoders";
                    for (unsigned int l = 7; l < command.size(); l++) command[l] = 0;
                } else {
                    for (unsigned int l = 7; l < command.size(); l++) {
                        command[l] = 10 * (ref_command[l] - encodersUsed[l]);
                        if (command[j_idx] > 20) command[j_idx] = 20;
                        if (command[j_idx] < -20) command[j_idx] = -20;
                    }
                }
            } else {
                yError("Can't babble the required body part.");
            }
        }

        if (part == "right_arm") {
            velRightArm->velocityMove(command.data());
        } else if (part == "left_arm") {
            velLeftArm->velocityMove(command.data());
        } else {
            yError() << "Don't know which part to move to do babbling.";
        }

        // This delay is needed!!!
        yarp::os::Time::delay(0.05);

    } else {
        yError() << "Which arm?";
    }

    return command;
}

bool Babbling::moveHeadToStartPos() {
    yarp::sig::Vector ang = start_commandHead;
    if (part == "right_arm") {
        ang[0] = -ang[0];
    }
    return igaze->lookAtAbsAngles(ang);
}

bool Babbling::gotoStartPos() {
    igaze->stopControl();
    velLeftArm->stop();
    velRightArm->stop();

    yarp::os::Time::delay(2.0);

    moveHeadToStartPos();

    /* Move arm to start position */
    if (part == "left_arm" || part == "right_arm") {
        if (part == "left_arm") {
            command = encodersLeftArm;
            for (int i = 0; i < 16; i++) {
                ictrlLeftArm->setControlMode(i, VOCAB_CM_POSITION);
                command[i] = start_command[i];
            }
            posLeftArm->positionMove(command.data());
        } else {
            command = encodersRightArm;
            for (int i = 0; i < 16; i++) {
                ictrlRightArm->setControlMode(i, VOCAB_CM_POSITION);
                command[i] = start_command[i];
            }
            posRightArm->positionMove(command.data());
        }

        bool done_head = false;
        bool done_arm = false;
        while (!done_head || !done_arm) {
            yInfo() << "Wait for position moves to finish";
            igaze->checkMotionDone(&done_head);
            if (part == "left_arm") {
                done_arm = true;
                for (int i = 0; i < 8; i++) {
                    bool done_joint = false;
                    posLeftArm->checkMotionDone(i, &done_joint);
                    if (!done_joint) {
                        done_arm = false;
                        break;
                    }
                }
            } else {
                posRightArm->checkMotionDone(&done_arm);
            }
            Time::delay(0.04);
        }
        yInfo() << "Done.";

        Time::delay(1.0);
    } else
        yError() << "Don't know which part to move to start position.";

    return true;
}

bool Babbling::init_iCub(string &part) {
    /* Create PolyDriver for left arm */
    Property option_left;

    string portnameLeftArm = "left_arm";  // part;
    option_left.put("robot", robot.c_str());
    option_left.put("device", "remote_controlboard");
    Value &robotnameLeftArm = option_left.find("robot");

    string sA;
    sA = "/babbling/" + robotnameLeftArm.asString() + "/" + portnameLeftArm + "/control";
    option_left.put("local", sA.c_str());

    sA = "/" + robotnameLeftArm.asString() + "/" + portnameLeftArm;
    option_left.put("remote", sA.c_str());

    yDebug() << "option left arm: " << option_left.toString().c_str();

    if (!leftArmDev.open(option_left)) {
        yError() << "Device not available. Here are the known devices:";
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
    for (int l = 0; l < 16; l++) {
        ictrlLimLeftArm->getLimits(l, &minLimArm[l], &maxLimArm[l]);
    }

    for (int l = 0; l < 16; l++) {
        yInfo() << "Joint " << l << ": limits = [" << minLimArm[l] << ","
                << maxLimArm[l] << "]. start_commad = " << start_command[l];
    }

    if (posLeftArm == NULL || encsLeftArm == NULL || velLeftArm == NULL ||
            itrqLeftArm == NULL || ictrlLeftArm == NULL || encsLeftArm == NULL) {
        yError() << "Cannot get interface to robot device for left arm";
        leftArmDev.close();
    }

    int nj = 0;
    posLeftArm->getAxes(&nj);
    velLeftArm->getAxes(&nj);
    itrqLeftArm->getAxes(&nj);
    encsLeftArm->getAxes(&nj);
    encodersLeftArm.resize(nj);

    yInfo() << "Wait for arm encoders";
    while (!encsLeftArm->getEncoders(encodersLeftArm.data())) {
        Time::delay(0.1);
        yInfo() << "Wait for arm encoders";
    }

    /* Create PolyDriver for right arm */
    Property option_right;

    string portnameRightArm = "right_arm";
    option_right.put("robot", robot.c_str());
    option_right.put("device", "remote_controlboard");
    Value &robotnameRightArm = option_right.find("robot");

    string sAr = "/babbling/" + robotnameRightArm.asString() + "/" + portnameRightArm + "/control";
    option_right.put("local", sAr.c_str());

    sAr = "/" + robotnameRightArm.asString() +  "/" + portnameRightArm;
    option_right.put("remote", sAr.c_str());

    yDebug() << "option right arm: " << option_right.toString().c_str();

    if (!rightArmDev.open(option_right)) {
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

    for (int l = 0; l < 16; l++) {
        ictrlLimRightArm->getLimits(l, &minLimArm[l], &maxLimArm[l]);
    }

    for (int l = 0; l < 16; l++) {
        yInfo() << "Joint " << l << ": limits = [" << minLimArm[l] << ","
                << maxLimArm[l] << "]. start_commad = " << start_command[l];
    }

    if (posRightArm == NULL || encsRightArm == NULL || velRightArm == NULL ||
            itrqRightArm == NULL || ictrlRightArm == NULL || encsRightArm == NULL) {
        yError() << "Cannot get interface to robot device for right arm";
        rightArmDev.close();
    }

    nj = 0;
    posRightArm->getAxes(&nj);
    velRightArm->getAxes(&nj);
    itrqRightArm->getAxes(&nj);
    encsRightArm->getAxes(&nj);
    encodersRightArm.resize(nj);

    yInfo() << "Wait for arm encoders";
    while (!encsRightArm->getEncoders(encodersRightArm.data())) {
        Time::delay(0.1);
        yInfo() << "Wait for arm encoders";
    }

    yInfo() << "Arms initialized.";

    /* Init. head */
    option_left.clear();
    option_left.put("device", "gazecontrollerclient");
    option_left.put("remote", "/iKinGazeCtrl");
    option_left.put("local", "/babbling/gaze");

    if (!headDev.open(option_left)) {
        yError() << "Device not available.  Here are the known devices:";
        yError() << yarp::dev::Drivers::factory().toString();
        return false;
    }

    headDev.view(igaze);

    yInfo() << "Head initialized.";


    yInfo() << "> Initialisation done.";

    return true;
}

bool Babbling::dealABM(const Bottle &command, bool begin) {
    yDebug() << "Dealing with ABM: bottle received = " << command.toString()
             << " of size = " << command.size() << " begin: " << begin;

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
    if (command.size() >= 3) {
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
    // 4 optional is already done if needed
    bSubArgument.addList() = bSubSubArgument5;
    bSubArgument.addList() = bSubSubArgument6;
    bSubArgument.addList() = bSubSubArgument7;

    bABM.addList() = bSubArgument;
    bABM.addList() = bBegin;
    yInfo() << "Bottle to ABM: " << bABM.toString();

    if (!Network::isConnected(portToABM.getName(),
                              "/autobiographicalMemory/rpc")) {
        Network::connect(portToABM.getName(), "/autobiographicalMemory/rpc");
    }
    if (Network::isConnected(portToABM.getName(),
                             "/autobiographicalMemory/rpc")) {
        portToABM.write(bABM, bABMreply);
    }

    yDebug() << "Finished dealing with ABM";

    if (bABMreply.isNull()) {
        yWarning() << "Reply from ABM is null : NOT connected?";
        return false;
    } else if (bABMreply.get(0).asString() == "nack") {
        yWarning() << "Got nack from ABM: " << bABMreply.toString();
        return false;
    } else {
        return true;
    }
}