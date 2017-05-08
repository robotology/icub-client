#include "homeostasisManagerIcub.h"
#include <cmath>

bool HomeostaticModule::addNewDrive(string driveName, yarp::os::Bottle& grpHomeostatic)
{    
    yDebug() << "Add drive " + driveName;
    Drive* drv = new Drive(driveName, period);

    drv->setHomeostasisMin(grpHomeostatic.check((driveName + "-homeostasisMin"), Value(drv->homeostasisMin)).asDouble());
    drv->setHomeostasisMax(grpHomeostatic.check((driveName + "-homeostasisMax"), Value(drv->homeostasisMax)).asDouble());
    drv->setDecay(grpHomeostatic.check((driveName + "-decay"), Value(drv->decay)).asDouble());
    drv->setValue((drv->homeostasisMax + drv->homeostasisMin) / 2.);
    drv->setGradient(grpHomeostatic.check((driveName + "-gradient"), Value(drv->gradient)).asInt());
    drv->setKey(grpHomeostatic.check((driveName + "-key"), Value(drv->key)).asString());
    
    manager->addDrive(drv);
   
    openPorts(driveName);

    return true;
}
bool HomeostaticModule::addNewDrive(string driveName)
{

    yInfo() << "adding a default drive..." ;
    Drive* drv = new Drive(driveName, period);


    drv->setHomeostasisMin(drv->homeostasisMin);
    drv->setHomeostasisMax(drv->homeostasisMax);
    drv->setDecay(drv->decay);
    drv->setValue((drv->homeostasisMax + drv->homeostasisMin) / 2.);
    drv->setGradient(false);

    manager->addDrive(drv);
    
    yDebug() << "default drive added. Opening ports...";//;
    openPorts(driveName);
    yInfo() << "new drive created successfuly!";//;

    return true;
}

int HomeostaticModule::openPorts(string driveName)
{
    //Create Ports
    string portName = "/" + moduleName + "/" + driveName;
    
    string pn = portName + ":i";
    //input_ports.push_back(new BufferedPort<Bottle>());
    outputm_ports.push_back(new BufferedPort<Bottle>());
    outputM_ports.push_back(new BufferedPort<Bottle>());

    /*yInfo() << "Configuring port " << " : " << pn << " ..." ;
    if (!input_ports.back()->open(pn)) 
    {
        yInfo() << getName() << ": Unable to open port " << pn ;
    }*/
    
    pn = portName + "/min:o";
    yInfo() << "Configuring port " << " : "<< pn << " ..." ;
    if (!outputm_ports.back()->open(pn)) 
    {
        yInfo() << getName() << ": Unable to open port " << pn ;
    }

    pn = portName + "/max:o";
    yInfo() << "Configuring port " << " : "<< pn << " ..." ;
    if (!outputM_ports.back()->open(pn)) 
    {
        yInfo() << getName() << ": Unable to open port " << pn ;
    }

    return 42;
}

bool HomeostaticModule::configure(yarp::os::ResourceFinder &rf)
{
    moduleName = rf.check("name",Value("homeostasis")).asString();
    setName(moduleName.c_str());

    verbose = rf.check("verbose",Value("false")).asBool();
    yInfo()<< "-- Set verbose mode to "<<verbose<<" -- ";

    yInfo()<<moduleName<<": finding configuration files...";
    period = rf.check("period",Value(0.1)).asDouble();

    manager = new HomeostasisManager();

    yInfo() << "Initializing drives";
    Bottle grpHomeostatic = rf.findGroup("HOMEOSTATIC");
    Bottle *drivesList = grpHomeostatic.find("drives").asList();
    if (drivesList)
    {
        yInfo() << "Configuration: Found " << drivesList->size() << " drives. " ;
        for (int d = 0; d<drivesList->size(); d++)
        {
            //Read Drive Configuration
            string driveName = drivesList->get(d).asString();
            addNewDrive(driveName, grpHomeostatic);
            
        }
    }

    yInfo() << "Opening RPC...";
     
    rpc.open ( ("/"+moduleName+"/rpc"));
    attach(rpc);

    yInfo()<<"Configuration done.";
    return true;
}

bool HomeostaticModule::removeDrive(int d)
{
    //Remove drive
    manager->removeDrive(d);
    //close ports
    input_ports[d]->close();
    outputm_ports[d]->close();
    outputM_ports[d]->close();
    delete input_ports[d];
    delete outputm_ports[d];
    delete outputM_ports[d];
    //Remove ports from lists
    input_ports.erase(input_ports.begin()+d);
    outputm_ports.erase(outputm_ports.begin()+d);
    outputM_ports.erase(outputM_ports.begin()+d);
    return true;
}


bool HomeostaticModule::respond(const Bottle& cmd, Bottle& reply)
{
    yInfo() << "RPC received: " << cmd.toString();
    bool all_drives = false;
    if (cmd.get(0).asString() == "help" )
    {   string help = "\n";
        help += " ['par'] ['drive'] ['val'/'min'/'max'/'dec'] [value]   : Assigns a value to a specific parameter \n";
        help += " ['delta'] ['drive'] ['val'/'min'/'max'/'dec'] [value] : Adds a value to a specific parameter  \n";
        help += " ['add'] ['conf'] [drive Bottle]                       : Adds a drive to the manager as a drive directly read from conf-file  \n";
        help += " ['add'] ['botl'] [drive Bottle]                       : Adds a drive to the manager as a Bottle of values of shape \n";
        help += " ['add'] ['new'] [drive name]                          : Adds a default drive to the manager \n";
        help += " ['rm'] [drive name]                                   : removes a drive from the manager \n";
        help += " ['sleep'] [drive name] [time]                         : prevent drive update for a certain time (in seconds) \n";
        help += " ['sleep'] ['all'] [time]                              : prevent all drive updates for a certain time (in seconds) \n";
        help += " ['names']                                             : returns an ordered list of the drives in the manager \n";
        help += " ['verbose'] [true/false]                              : change verbosity \n";
        help += "                                                       : (string name, double value, double homeo_min, double homeo_max, double decay = 0.05, bool gradient = true) \n";
        reply.addString(help);
        
    }    
    else if (cmd.get(0).asString() == "par" )
    {
        
        if (cmd.get(1).asString() == "all") {
            all_drives = true;
        }
        for (unsigned int d = 0; d<manager->drives.size();d++)
        {
            if (all_drives || cmd.get(1).asString() == manager->drives[d]->name)
            {
                if (cmd.get(2).asString()=="val")
                {
                    manager->drives[d]->setValue(cmd.get(3).asDouble());
                }
                else if (cmd.get(2).asString()=="min")
                {
                    manager->drives[d]->setHomeostasisMin(cmd.get(3).asDouble());
                }
                else if (cmd.get(2).asString()=="max")
                {
                    manager->drives[d]->setHomeostasisMax(cmd.get(3).asDouble());
                }
                else if (cmd.get(2).asString()=="dec")
                {
                    manager->drives[d]->setDecay(cmd.get(3).asDouble());
                }
                else if (cmd.get(2).asString()=="decaymult")
                {
                    manager->drives[d]->setDecayMultiplier(cmd.get(3).asDouble());
                }                
                else if (cmd.get(2).asString()=="reset")
                {
                    manager->drives[d]->reset();
                }
                else
                {
                    reply.addString("Format is: \n - ['par'] [drive_name] [val/min/max/dec/reset] [value]");
                    
                }
                reply.addString("ack");
            }

        }
        reply.addString("nack: wrong drive name");
    }
    else if (cmd.get(0).asString() == "delta" )
    {
        for (unsigned int d = 0; d<manager->drives.size();d++)
        {
            if (cmd.get(1).asString() == manager->drives[d]->name)
            {
                if (cmd.get(2).asString()=="val")
                {
                    manager->drives[d]->deltaValue(cmd.get(3).asDouble());
                }
                else if (cmd.get(2)=="min")
                {
                    manager->drives[d]->deltaHomeostasisMin(cmd.get(3).asDouble());
                }
                else if (cmd.get(2)=="max")
                {
                    manager->drives[d]->deltaHomeostasisMax(cmd.get(3).asDouble());
                }
                else if (cmd.get(2)=="dec")
                {
                    manager->drives[d]->deltaDecay(cmd.get(3).asDouble());
                }
                else
                {
                    reply.addString("nack");
                    yInfo() << "Format is: \n - ['par'] [drive_name] [val/min/max/dec] [value]";
                }
                reply.addString("ack");
            }
        }
    }
    else if (cmd.get(0).asString()=="add")
    {
        if (cmd.get(1).asString()=="conf")
        {
            Bottle *ga = cmd.get(2).asList();
            Bottle grpAllostatic = ga->findGroup("ALLOSTATIC");

            addNewDrive(cmd.get(2).check("name",yarp::os::Value("")).asString(), grpAllostatic);
            reply.addString("add drive from config bottle: ack");
        }
        else if (cmd.get(1).asString()=="botl")
        {
            Drive d = bDrive(cmd.get(2).asList());
            manager->addDrive(&d);
            openPorts(d.name);
            reply.addString("add drive from bottle: ack");
        }
        else if (cmd.get(1).asString()=="new")
        {
            string d_name = cmd.get(2).asString();
            yInfo() << "adding new drive... " ;
            bool b = addNewDrive(d_name);
            if (b)
                reply.addString("add new drive: ack");
            else
                reply.addString("ack. drive not created");
        }
        else
        {
            reply.addString("nack");
        }

    }
    else if (cmd.get(0).asString()=="rm")
    {
        for (unsigned int d = 0; d<manager->drives.size();d++)
        {
            if (cmd.get(1).asString() == manager->drives[d]->name)
            {
                bool b = removeDrive(d);
                if (b)
                    reply.addString("ack: Successfuly removed");
                else
                    reply.addString("ack: Could not remove the drive");

            }
        }
        reply.addString("nack");
    }
    else if (cmd.get(0).asString()=="freeze" || cmd.get(0).asString()=="unfreeze") {
        if (cmd.get(1).asString() == "all") {
            all_drives = true;
        }
        for (unsigned int d = 0; d<manager->drives.size();d++)
        {
            if (all_drives || cmd.get(1).asString() == manager->drives[d]->name)
            {
                if (cmd.get(0).asString()=="freeze") {
                    manager->drives[d]->freeze(); 
                }
                else {
                    manager->drives[d]->unfreeze();
                }
            }
        }
        reply.addString("ack");
    }
    else if (cmd.get(0).asString()=="sleep")
    {
        if (cmd.get(1).asString() != "all")
        {
            for (unsigned int d = 0; d<manager->drives.size();d++)
            {
                if (cmd.get(1).asString() == manager->drives[d]->name)
                {
                    double t = cmd.get(2).asDouble();
                    manager->sleep(d, t);
                    reply.addString("ack: Drive sleep");
                }
            }
        }
        else
        {
            for (unsigned int d = 0; d<manager->drives.size();d++)
            {
                double t = cmd.get(2).asDouble();
                manager->sleep(d, t);
                std::stringstream ss;
                ss << "ack: "<< manager->drives[d]->name << " sleep for " << t << "s";
                reply.addString(ss.str());
            }
        }
        reply.addString("nack");
    }    
    else if (cmd.get(0).asString()=="names")
    {
        Bottle nms;
        nms.clear();
        for (unsigned int i=0;i<manager->drives.size();i++)
        {
            nms.addString(manager->drives[i]->name);
        }
        reply.addList()=nms;
    }
    else if (cmd.get(0).asString() == "ask")
    {
        for (auto drive: manager->drives)
        {
            if (cmd.get(1).asString() == drive->name)
            {
                if (cmd.get(2).asString() == "min") {
                    reply.addDouble(drive->homeostasisMin);
                } else if (cmd.get(2).asString() == "max") {
                    reply.addDouble(drive->homeostasisMax);
                } else {
                    reply.addString("nack");
                }
            }
        }
        if (reply.size() == 0) {
            reply.addString("nack");
        }
    }
    else if (cmd.get(0).asString() == "force")
    {
        for (unsigned int d = 0; d<manager->drives.size();d++)
        {
            manager->drives[d]->freeze();
            manager->drives[d]->reset();
            if (cmd.get(1).asString() == manager->drives[d]->name)
            {
                if (cmd.get(2).asString()=="bottom")
                    manager->drives[d]->setValue(0.);
                else if (cmd.get(2).asString()=="top")
                    manager->drives[d]->setValue(1.);
                else
                {
                    reply.addString("Format is: \n - ['force'] [drive_name] [top/bottom]");
                }
                reply.addString("ack");
            }
            else
            {
                reply.addString("Format is: \n - ['force'] [drive_name] [top/bottom]");
            }

        }
    }else if (cmd.get(0).asString() == "verbose")
    {
        if (cmd.size()!=2)
            yWarning()<<"The command should be 'verbose true/false'";
        else
        {
            verbose = cmd.get(1).asBool();
        }
    }  
    else
    {
        reply.addString("nack");
    }
    return true;
}

bool HomeostaticModule::updateModule()
{
    yarp::os::Bottle* sens_input = input_port.read(false);
    for(unsigned int d = 0; d<manager->drives.size();d++)
    {
        if (verbose)
        {
            yInfo() << "Going by drive #"<<d << " with name "<< manager->drives[d]->name ;
        }

        double inp;
        inp = sens_input->find(manager->drives[d]->key).asDouble();
        
        // [CLEANUP/] should we keep this?
        if(manager->drives[d]->gradient == true)
        {
            if (inp)
            {
                manager->drives[d]->setValue(inp);
            }else{
                //manager->drives[d]->setValue(inp->get(0).asDouble());
            }
        }
        // [/CLEANUP]

        manager->drives[d]->update();
        yarp::os::Bottle &out1 = outputm_ports[d]->prepare();// = output_ports[d]->prepare();
        out1.clear();
        yarp::os::Bottle &out2 = outputM_ports[d]->prepare();
        out2.clear();
        out1.addDouble(-manager->drives[d]->getValue()+manager->drives[d]->homeostasisMin);
        outputm_ports[d]->write();
        if (verbose)
        {
            yInfo() <<"Drive value: " << manager->drives[d]->value;
            yInfo() <<"Drive decay: " << manager->drives[d]->decay;
            yInfo() <<"Drive homeostasisMin: " << manager->drives[d]->homeostasisMin;
            yInfo() <<"Drive homeostasisMax: " << manager->drives[d]->homeostasisMax;
            yInfo() <<"Drive gradient: " << manager->drives[d]->gradient;
            yInfo() <<"Drive period: " << manager->drives[d]->period;
            yInfo()<<out1.get(0).asDouble();
        }

        out2.addDouble(+manager->drives[d]->getValue()-manager->drives[d]->homeostasisMax);
        outputM_ports[d]->write();

    }
    return true;
}

bool HomeostaticModule::close()
{
    for (unsigned int d=0; d<manager->drives.size(); d++)
    {
        input_ports[d]->interrupt();
        input_ports[d]->close();
        delete input_ports[d];

        outputM_ports[d]->interrupt();
        outputM_ports[d]->close();
        delete outputM_ports[d];

        outputm_ports[d]->interrupt();
        outputm_ports[d]->close();
        delete outputm_ports[d];

        delete manager->drives[d];
    }

    input_ports.clear();
    outputM_ports.clear();
    outputm_ports.clear();

    rpc.interrupt();
    rpc.close();

    delete manager;

    return true;
}
