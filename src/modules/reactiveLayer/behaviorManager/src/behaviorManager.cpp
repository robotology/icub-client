#include "behaviorManager.h"

#include "dummy.h"
#include "tagging.h"
#include "pointing.h"
#include "moveObject.h"

using namespace std;
using namespace yarp::os;

bool BehaviorManager::interruptModule()
{
    rpc_in_port.interrupt();

    for(auto& beh : behaviors) {
        beh->interrupt_ports();
    }
    return true;
}

bool BehaviorManager::close()
{
    rpc_in_port.interrupt();
    rpc_in_port.close();
    iCub->close();

    for(auto& beh : behaviors) {
        beh->interrupt_ports();
        beh->close_ports();
        delete beh;
    }
    behaviors.clear();

    delete iCub;

    return true;
}

bool BehaviorManager::configure(yarp::os::ResourceFinder &rf)
{
    moduleName = rf.check("name",Value("BehaviorManager")).asString();
    setName(moduleName.c_str());
    yInfo()<<moduleName<<": finding configuration files...";//<<endl;
    period = rf.check("period",Value(1.0)).asDouble();

    Bottle grp = rf.findGroup("BEHAVIORS");
    Bottle behaviorList = *grp.find("behaviors").asList();

    rpc_in_port.open("/" + moduleName + "/trigger:i");
    yInfo() << "RPC_IN : " << rpc_in_port.getName();

    //Create an iCub Client and check that all dependencies are here before starting
    bool isRFVerbose = false;
    iCub = new icubclient::ICubClient(moduleName, "behaviorManager","client.ini",isRFVerbose);

    if (!iCub->connect())
    {
        yInfo() << "iCubClient : Some dependencies are not running...";
        Time::delay(1.0);
    }

    for (int i = 0; i<behaviorList.size(); i++)
    {
        std::string behavior_name = behaviorList.get(i).asString();
        if (behavior_name == "tagging") {
            behaviors.push_back(new Tagging(&mut, rf, iCub, "tagging"));
        } else if (behavior_name == "pointing") {
            behaviors.push_back(new Pointing(&mut, rf, iCub, "pointing"));
        } else if (behavior_name == "dummy") {
            behaviors.push_back(new Dummy(&mut, rf, iCub, "dummy"));
        } else if (behavior_name == "dummy2") {
            behaviors.push_back(new Dummy(&mut, rf, iCub, "dummy2"));
        }  else if (behavior_name == "moveObject") {
            behaviors.push_back(new MoveObject(&mut, rf, iCub, "moveObject"));
            // other behaviors here
        }  else {
            yDebug() << "Behavior " + behavior_name + " not implemented";
            return false;
        }
    }

    while (!Network::connect("/ears/behavior:o", rpc_in_port.getName())) {
        yWarning() << "Ears is not reachable";
        yarp::os::Time::delay(0.5);
    }

    for(auto& beh : behaviors) {
        beh->configure();
        beh->openPorts(moduleName);

        if (beh->from_sensation_port_name != "None") {
            while (!Network::connect(beh->from_sensation_port_name, beh->sensation_port_in.getName())) {
                yInfo()<<"Connecting "<< beh->from_sensation_port_name << " to " <<  beh->sensation_port_in.getName();// <<endl;
                yarp::os::Time::delay(0.5);
            }
        }
        if (beh->external_port_name != "None") {
            while (!Network::connect(beh->rpc_out_port.getName(), beh->external_port_name)) {
                yInfo()<<"Connecting "<< beh->rpc_out_port.getName() << " to " <<  beh->external_port_name;// <<endl;
                yarp::os::Time::delay(0.5);
            }   
        }

    }

    attach(rpc_in_port);
    yInfo("Init done");

    return true;
}


bool BehaviorManager::updateModule()
{
    return true;
}


bool BehaviorManager::respond(const Bottle& cmd, Bottle& reply)
{
    yDebug() << "RPC received in BM";
    yDebug() << cmd.toString();
    
    reply.clear();

    if (cmd.get(0).asString() == "help" )
    {   string help = "\n";
        help += " ['behavior_name']  : Triggers corresponding behavior \n";
        reply.addString(help);
    }
    else if (cmd.get(0).asString() == "names" ) {
        Bottle names;
        for(auto& beh : behaviors) {
            names.addString(beh->behaviorName);
        }
        reply.addList() = names;
    } else if (cmd.get(0).asString() == "is_available" ) {
        if (mut.tryLock()) {
            mut.unlock();
            reply.addInt(1);
        } else {
            reply.addInt(0);
        }
    }    
    else
    {
        bool behavior_triggered = false;
        for(auto& beh : behaviors) {
            if (cmd.get(0).asString() == beh->behaviorName) {
                bool aux;
                if (beh->from_sensation_port_name != "None") {
                    if (!Network::isConnected(beh->from_sensation_port_name, beh->sensation_port_in.getName())) {
                        aux =Network::connect(beh->from_sensation_port_name, beh->sensation_port_in.getName());
                        yInfo()<< "The sensations port for "<< beh->behaviorName <<" was not connected. \nReconnection status: " << aux;
                    }
                }
                if (beh->external_port_name != "None") {
                    if (!Network::isConnected(beh->rpc_out_port.getName(), beh->external_port_name)) {
                        aux = Network::connect(beh->rpc_out_port.getName(), beh->external_port_name);
                        yInfo()<< "The external port for "<< beh->behaviorName <<" was not connected. \nReconnection status: " << aux;
                    }   
                }

                Bottle args;
                if (cmd.size()>1){
                    args = cmd.tail();
                }
                yDebug() << "arguments are " << args.toString().c_str();
                behavior_triggered = beh->trigger(args);
            }
        }
        if (behavior_triggered) {
            reply.addString("ack");
        } else {
            reply.addString("nack");
            yDebug()<< "Behavior ' " << cmd.get(0).asString() << " ' cant be triggered. \nSend 'names' to see a list of available behaviors. ";
        }
    }
    yDebug() << "End of BehaviorManager::respond";
    return true;
}
