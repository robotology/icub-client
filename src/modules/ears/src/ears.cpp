#include <yarp/os/all.h>
#include "ears.h"
#include "icubclient/clients/icubClient.h"
#include "icubclient/clients/opcClient.h"
#include "icubclient/functions.h"
#include "icubclient/subsystems/subSystem_recog.h"

using namespace std;
using namespace icubclient;
using namespace yarp::os;

bool ears::configure(yarp::os::ResourceFinder &rf)
{
    string moduleName = rf.check("name", Value("ears")).asString().c_str();
    setName(moduleName.c_str());

    yInfo() << moduleName << " : finding configuration files...";
    period = rf.check("period", Value(0.1)).asDouble();

    //Create an iCub Client and check that all dependencies are here before starting
    bool isRFVerbose = false;
    iCub = new ICubClient(moduleName, "ears", "client.ini", isRFVerbose);
    iCub->opc->isVerbose = false;
    if (!iCub->connect())
    {
        yInfo() << " iCubClient : Some dependencies are not running...";
        Time::delay(1.0);
    }

    portToSpeechRecognizer.open("/" + moduleName + "/speech:o");

    MainGrammar = rf.findFileByName(rf.check("MainGrammar", Value("MainGrammar.xml")).toString());
    bShouldListen = true;

    portToBehavior.open("/" + moduleName + "/behavior:o");

    rpc.open(("/" + moduleName + "/rpc").c_str());
    attach(rpc);

    yInfo() << "\n \n" << "----------------------------------------------" << "\n \n" << moduleName << " ready ! \n \n ";

    return true;
}

bool ears::interruptModule() {
    // speechRecognizer is in a long loop, which prohibits closure of ears
    // so interrupt the speechRecognizer
    yDebug() << "interrupt ears";
    bShouldListen = false;
    if(Network::connect("/" + getName() + "/speech:o", "/speechRecognizer/rpc")) {
        Bottle bMessenger, bReply;
        bMessenger.addString("interrupt");
        // send the message
        portToSpeechRecognizer.write(bMessenger, bReply);
        if(bReply.get(1).asString() != "OK") {
            yError() << "speechRecognizer was not interrupted";
            yDebug() << "Reply from speechRecognizer:" << bReply.toString();
        }
    }

    yDebug() << "interrupted speech recognizer";
    portToSpeechRecognizer.interrupt();
    portToBehavior.interrupt();
    rpc.interrupt();

    yDebug() << "interrupt done";

    return true;
}


bool ears::close() {
    yDebug() << "close ears";

    if(iCub) {
        iCub->close();
        delete iCub;
    }
    yDebug() << "closed icub";

    portToSpeechRecognizer.interrupt();
    portToSpeechRecognizer.close();

    portToBehavior.interrupt();
    portToBehavior.close();

    yDebug() << "closing rpc port";
    rpc.interrupt();
    rpc.close();

    yDebug() << "end of close. bye!";
    return true;
}


bool ears::respond(const Bottle& command, Bottle& reply) {
    string helpMessage = string(getName().c_str()) +
            " commands are: \n" +
            "quit \n";

    reply.clear();

    if (command.get(0).asString() == "quit") {
        reply.addString("quitting");
        return false;
    }
    else if (command.get(0).asString() == "listen")
    {
        if (command.size() == 2)
        {
            if (command.get(1).asString() == "on")
            {
                yDebug() << "should listen on";
                bShouldListen = true;
                reply.addString("ack");
            }
            else if (command.get(1).asString() == "off")
            {
                yDebug() << "should listen off";
                bShouldListen = false;
                reply.addString("ack");
            }
            else if (command.get(1).asString() == "offShouldWait")
            {
                yDebug() << "should listen offShouldWait";
                bShouldListen = false;
                LockGuard lg(m);
                reply.addString("ack");
            }
            else {
                reply.addString("nack");
                reply.addString("Send either listen on or listen off");
            }
        }
    }
    else {
        yInfo() << helpMessage;
        reply.addString("wrong command");
    }

    return true;
}

/* Called periodically every getPeriod() seconds */
bool ears::updateModule() {
    if (bShouldListen)
    {
        LockGuard lg(m);
        yDebug() << "bListen";
        Bottle bRecognized, //received FROM speech recog with transfer information (1/0 (bAnswer))
                bAnswer, //response from speech recog without transfer information, including raw sentence
                bSemantic; // semantic information of the content of the recognition
        bRecognized = iCub->getRecogClient()->recogFromGrammarLoop(grammarToString(MainGrammar), 1, true, true);

        if (bRecognized.get(0).asInt() == 0)
        {
            yDebug() << "ears::updateModule -> speechRecognizer did not recognize anything";
            return true;
        }

        bAnswer = *bRecognized.get(1).asList();

        if (bAnswer.get(0).asString() == "stop")
        {
            yInfo() << " in ears::updateModule | stop called";
            return true;
        }
        // bAnswer is the result of the regognition system (first element is the raw sentence, 2nd is the list of semantic element)

        if(bAnswer.get(1).asList()->get(1).isList()) {
            bSemantic = *(*bAnswer.get(1).asList()).get(1).asList();
        }
        string sObject, sAction;
        string sQuestionKind = bAnswer.get(1).asList()->get(0).toString();

        // forward command appropriately to behaviorManager
        string sObjectType, sCommand;
        if(sQuestionKind == "SENTENCEOBJECT") {
            sAction = bSemantic.check("predicateObject", Value("none")).asString();
            if (sAction == "please take") {
                sAction = "back";
                sCommand = "moveObject";
            }
            else if (sAction == "give me") {
                sAction = "front";
                sCommand = "moveObject";
            }
            else if (sAction == "point") {
                sAction = "pointing";
                sCommand = "pointing";
            }
            sObjectType = "object";
            sObject = bSemantic.check("object", Value("none")).asString();
        } else if (sQuestionKind == "SENTENCERECOGNISE") {
            sCommand = "recognitionOrder";
            sAction = "recognitionOrder";
            sObjectType = "";
            sObject = "";
        } else {
            yError() << "[ears] Unknown predicate: " << sQuestionKind;
            return true;
        }

        Bottle bAction,bArgs;
        // object might not be known yet, tag it first
        if (sObject!="") {
            bAction.addString("tagging");
            bArgs.addString(sObject);
            bArgs.addString(sAction);
            bArgs.addString(sObjectType);
            bAction.addList()=bArgs;
            portToBehavior.write(bAction);
            yDebug() << "Sending " + bAction.toString();
        }

        // now execute actual behavior
        bAction.clear();
        bAction.addString(sCommand);
        bArgs.addString(sObject);
        bArgs.addString(sAction);
        bArgs.addString(sObjectType);
        bAction.addList()=bArgs;
        portToBehavior.write(bAction);
        yDebug() << "Sending " + bAction.toString();
    } else {
        yDebug() << "Not bShouldListen";
        yarp::os::Time::delay(0.5);
    }

    return true;
}
