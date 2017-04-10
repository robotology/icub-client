/* 
 * Copyright (C) 2014 WYSIWYD Consortium, European Commission FP7 Project ICT-612139
 * Authors: Stéphane Lallée
 * email:   stephane.lallee@gmail.com
 * website: https://github.com/robotology/icub-client/
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


#include "icubclient/clients/opcClient.h"

#include <yarp/os/all.h>
#include <yarp/sig/all.h>
#include <yarp/dev/all.h>
#include <iostream>
#include <string>
#include <map>
#include <list>

using namespace std;
using namespace yarp::os;
using namespace yarp::sig;
using namespace yarp::dev;
using namespace icubclient;

OPCClient::OPCClient(const string &moduleName)
{
    opc.open("/" + moduleName + "/world/opc:rpc");

    isVerbose = false;
}    

bool OPCClient::write(Bottle &cmd, Bottle &reply, bool Verbose)
{
    if (opc.getOutputCount() > 0)
    {
        if (Verbose)
            yDebug()<<"Sending to OPC: "<<cmd.toString().c_str();

        opc.write(cmd,reply);

        if (Verbose)
            yDebug()<<"Receiving from OPC: "<<reply.toString().c_str();

        if (reply.get(0).asVocab() == VOCAB4('n','a','c','k'))
            return false;
        else
            return true;
    }
    else
    {
        if (Verbose)
            yWarning()<<"Not connected to OPC...";
        return false;
    }
}

void OPCClient::interrupt()
{
    opc.interrupt();
}

void OPCClient::close()
{
    opc.close();
    //Clear the content of dictionaries
    for(map<int,Entity*>::iterator it=entitiesByID.begin(); it!=entitiesByID.end(); it++)
    {
        delete it->second;
    }
    this->entitiesByID.clear();
}

void OPCClient::clear()
{
    Bottle cmd,reply;
    cmd.addVocab(Vocab::encode("del"));
    cmd.addList().addString("all");
    
    //Clear the content of dictionaries
    for(map<int,Entity*>::iterator it=entitiesByID.begin(); it!=entitiesByID.end(); it++)
    {
        delete it->second;
    }
    this->entitiesByID.clear();
    write(cmd,reply);
}

void OPCClient::addEntity(Entity* e)
{
    Bottle cmd,reply;
    cmd.addVocab(Vocab::encode("add"));
    Bottle& query = cmd.addList();
    query = e->asBottle();
    write(cmd,reply,isVerbose);

    if (reply.get(0).asVocab() == VOCAB4('n','a','c','k'))
    {
        yError() << "Impossible to communicate correctly with OPC";
        return;
    }

    //Update the assigned ID
    e->m_opc_id = reply.get(1).asList()->get(1).asInt();

    //Forces an update of the object
    update(e);

    //Commit to the dictionary
    entitiesByID[e->opc_id()] = e;
}

/**
* Try to assign a property from an entity to another entity, using a specific property name
 */
bool OPCClient::setEntityProperty(std::string sourceEntityName, std::string propertyName, std::string targetEntityName)
{
    Entity* source = getEntity(sourceEntityName,false);
    Entity* target = getEntity(targetEntityName,false);
    if (source == NULL || target == NULL)
        return false;
    
    source->m_properties[propertyName] = targetEntityName;
    return true;
}

//Get by name
Entity* OPCClient::getEntity(const string &name, bool forceUpdate)
{
    //If the object is already in the dictionary we return it.
    for(map<int,Entity*>::iterator it=entitiesByID.begin(); it!=entitiesByID.end(); it++) {
        if(it->second->name() == name) {
            if (forceUpdate)
                update(it->second);
            return it->second;
        }
    }

    //Else we try to find the item in the OPC
    Bottle cmd, reply;
    cmd.addVocab(Vocab::encode("ask"));
    Bottle& query = cmd.addList();

    Bottle& sub = query.addList();
    sub.addString("name");
    sub.addString("==");
    sub.addString(name.c_str());

    write(cmd,reply,isVerbose);

    if (reply.get(0).asVocab() == VOCAB4('n','a','c','k'))
    {
        yError() << "Unable to talk correctly to OPC in getEntity with name";
        yError() << "Command was:" << cmd.toString();
        yError() << "Reply was:" << reply.toString();
        return NULL;
    }

    if (reply.get(1).asList()->get(1).asList()->size() == 0)
    {
        //yError() << "Entity doesn't exist yet. Use addEntity<>() first.";
        return NULL;
    }

    if (reply.get(1).asList()->get(1).asList()->size() > 1)
    {
        yError() << "[IMPORTANT] Duplicated names... You should fix this!";
    }

    int item_id = reply.get(1).asList()->get(1).asList()->get(0).asInt();
    return getEntity(item_id);
}

//Get by ID
Entity *OPCClient::getEntity(int id, bool forceUpdate)
{
    //If the object is already in the dictionary we return it.
    map<int, Entity*>::iterator itE = entitiesByID.find(id);
    if (itE != entitiesByID.end())
    {
        if (forceUpdate)
            update(itE->second);
        return itE->second;
    }

    Bottle cmd,reply;
    //Performs an update
    cmd.addVocab(Vocab::encode("get"));
    Bottle& query = cmd.addList();
    Bottle& sub = query.addList();
    sub.addString("id");
    sub.addInt(id);
    write(cmd,reply,isVerbose);

    if (reply.get(0).asVocab() == VOCAB4('n','a','c','k'))
    {
        yError() << "Unable to talk correctly to OPC in getEntity with ID";
        yError() << "Command was:" << cmd.toString();
        yError() << "Reply was:" << reply.toString();
        return NULL;
    }

    Entity* newE = NULL;
    std::string newEntityType = reply.get(1).asList()->find("entity").asString().c_str();

    //Cast to the right type
    if(newEntityType == "entity")
        newE = new Entity();
    else if (newEntityType == ICUBCLIENT_OPC_ENTITY_OBJECT)
        newE = new Object();
    else if (newEntityType == ICUBCLIENT_OPC_ENTITY_AGENT)
        newE = new Agent();
    else if (newEntityType == ICUBCLIENT_OPC_ENTITY_ACTION)
        newE = new Action();
    else if (newEntityType == "bodypart")
        newE = new Bodypart();
    else
        yError() << "getEntity: Unknown Entity type!";

    //Update the fields
    newE->fromBottle(*reply.get(1).asList());
    newE->m_opc_id = id;
    newE->m_original_entity = newE->asBottle();


    //Commit to the dictionary
    entitiesByID[newE->opc_id()] = newE;

    return newE;
}

bool OPCClient::removeEntity(const string &name)
{
    if (Entity *e=getEntity(name,true))
    {
        Bottle cmd,reply;
        cmd.addVocab(Vocab::encode("del"));
        Bottle &payLoad=cmd.addList().addList();
        payLoad.addString("id");
        payLoad.addInt(e->opc_id()); 

        if (write(cmd,reply,isVerbose))
        {
            if (reply.get(0).asVocab()==VOCAB3('a','c','k')) {
                entitiesByID.erase(e->opc_id());
                return true;
            }
            else {
                yError()<<"Unable to talk correctly to OPC in removeEntity by name for " << name << "(id " << e->opc_id() << ")";
                yError()<<"Command sent:" << cmd.toString();
                yError()<<"Reply:" << reply.toString();
            }
        }
    }

    return false;
}

bool OPCClient::removeEntity(int id)
{
    if (Entity *e=getEntity(id,true))
    {
        Bottle cmd,reply;
        cmd.addVocab(Vocab::encode("del"));
        Bottle &payLoad=cmd.addList().addList();
        payLoad.addString("id");
        payLoad.addInt(e->opc_id()); 

        if (write(cmd,reply,isVerbose))
        {
            if (reply.get(0).asVocab()==VOCAB3('a','c','k'))
            {
                entitiesByID.erase(e->opc_id());
                return true;
            }
            else {
                yError()<<"Unable to talk correctly to OPC in removeEntity by ID for id" << id << "(" << e->name() << ")";
                yError()<<"Command sent:" << cmd.toString();
                yError()<<"Reply:" << reply.toString();
            }
        }
    }

    return false;
}

//Returns the OPCid of a relation on the server side
int OPCClient::getRelationID(
        Entity* subject,
        Entity* verb,
        Entity* object,
        Entity* complement_place,
        Entity* complement_time,
        Entity* complement_manner)
{
    Relation r(subject,verb,object,complement_place,complement_time,complement_manner);
    //Check if the relation already exists on the server side (which means same source/target/type)
    Bottle cmd,reply;
    cmd.addString("ask");

    Bottle& query = cmd.addList();
    Bottle sub;

    sub.addString(ICUBCLIENT_OPC_ENTITY_TAG);
    sub.addString("==");
    sub.addString(ICUBCLIENT_OPC_ENTITY_RELATION);
    query.addList() = sub;

    query.addString("&&");

    sub.clear();
    sub.addString("rSubject");
    sub.addString("==");
    sub.addString(r.subject().c_str());
    query.addList() = sub;

    query.addString("&&");

    sub.clear();
    sub.addString("rVerb");
    sub.addString("==");
    sub.addString(r.verb().c_str());
    query.addList() = sub;

    query.addString("&&");

    sub.clear();
    sub.addString("rObject");
    sub.addString("==");
    sub.addString(r.object().c_str());
    query.addList() = sub;

    query.addString("&&");

    sub.clear();
    sub.addString("rCompPlace");
    sub.addString("==");
    sub.addString(r.complement_place().c_str());
    query.addList() = sub;

    query.addString("&&");

    sub.clear();
    sub.addString("rCompTime");
    sub.addString("==");
    sub.addString(r.complement_time().c_str());
    query.addList() = sub;

    query.addString("&&");

    sub.clear();
    sub.addString("rCompManner");
    sub.addString("==");
    sub.addString(r.complement_manner().c_str());
    query.addList() = sub;

    write(cmd,reply,isVerbose);

    if (reply.get(0).asVocab() == VOCAB4('n','a','c','k'))
    {
        yError()<<"Unable to talk correctly to OPC in getRelationID";
        yError() << "Command was:" << cmd.toString();
        yError() << "Reply was:" << reply.toString();
        return false;
    }
    
    //Relation already exists on the server
    if (reply.get(1).asList()->get(1).asList()->size() >= 1)
    {
        return (reply.get(1).asList()->get(1).asList()->get(0).asInt());
    }

    return -1;
}
//Adds a relation between 2 entities
bool OPCClient::addRelation(const Relation &r, double lifeTime)
{   
    Entity* subject = getEntity(r.subject());
    Entity* object = getEntity(r.object());
    Entity* cPlace = getEntity(r.complement_place());
    Entity* cTime = getEntity(r.complement_time());
    Entity* cManner = getEntity(r.complement_manner());
    Entity* verb = getEntity(r.verb());
    if (subject == NULL || verb == NULL)
    {
        yError() << "Verb and subject should exist before you try to add a relation";
        return false;
    }

    return addRelation(subject,verb,object,lifeTime,cPlace,cTime,cManner);
}

//Adds a relation between 2 entities
bool OPCClient::addRelation(
        Entity* subject,
        Entity* verb,
        Entity* object,
        double lifeTime,
        Entity* complement_place,
        Entity* complement_time,
        Entity* complement_manner)
{   

    //Check if the relation already exists on the server side (which means same source/target/type)
    int index = getRelationID(subject,verb,object,complement_place,complement_time,complement_manner);
    Bottle cmd,reply;
    //Relation already exists on the server, we just reset the lifeTimer
    if (index != -1)
    {
        if (isVerbose)
            yWarning() << "This relation already exist, only reseting lifeTimer";
        return setLifeTime(index,lifeTime);
    }

    //Relation has to be created
    Relation r(subject,verb,object,complement_place,complement_time,complement_manner);
    cmd.clear();
    cmd.addString("add");
    Bottle sub = r.asBottle(true); //ignoring the ID
    cmd.addList() = sub;

    write(cmd,reply,isVerbose);

    if (reply.get(0).asVocab() == VOCAB4('n','a','c','k'))
    {
        yError() << "Unable to talk correctly to OPC in addRelation";
        yError() << "Command was:" << cmd.toString();
        yError() << "Reply was:" << reply.toString();
        return false;
    }
    index = reply.get(1).asList()->get(1).asInt();
    return setLifeTime(index,lifeTime);
}

//Removes a relation between 2 entities
bool OPCClient::removeRelation(const Relation &r)
{   
    Entity* subject = getEntity(r.subject());
    Entity* object = getEntity(r.object());
    Entity* cPlace = getEntity(r.complement_place());
    Entity* cTime = getEntity(r.complement_time());
    Entity* cManner = getEntity(r.complement_manner());
    Entity* verb = getEntity(r.verb());
    return removeRelation(subject,verb,object,cPlace,cTime,cManner);
}

//Removes a relation between 2 entities
bool OPCClient::removeRelation(
        Entity* subject,
        Entity* verb,
        Entity* object,
        Entity* complement_place,
        Entity* complement_time,
        Entity* complement_manner)
{       

    int index = getRelationID(subject,verb,object,complement_place,complement_time,complement_manner);

    if (index == -1)
    {
        yWarning() << "This relation does not exist on the OPC server, not removed";
    }
    else
    {
        Bottle cmd,reply;
        Bottle sub ;
        cmd.addString("del");

        sub.addString("id");
        sub.addInt(index);

        cmd.addList().addList() = sub;

        write(cmd,reply,isVerbose);
        if (reply.get(0).asVocab() == VOCAB4('n','a','c','k'))
        {
            yError() << "Unable to talk correctly to OPC in removeRelation. Item not deleted.";
            yError() << "Command was:" << cmd.toString();
            yError() << "Reply was:" << reply.toString();
            return false;
        }
    }

    return true;
}

bool OPCClient::containsRelation(
        Entity* subject,
        Entity* verb,
        Entity* object,
        Entity* complement_place,
        Entity* complement_time,
        Entity* complement_manner)
{
    int index = getRelationID(subject,verb,object,complement_place,complement_time,complement_manner);
    return (index != -1);
}

bool OPCClient::containsRelation(const Relation &r)
{    
    Entity* subject = getEntity(r.subject());
    Entity* object = getEntity(r.object());
    Entity* cPlace = getEntity(r.complement_place());
    Entity* cTime = getEntity(r.complement_time());
    Entity* cManner = getEntity(r.complement_manner());
    Entity* verb = getEntity(r.verb());
    return containsRelation(subject,verb,object,cPlace,cTime,cManner);
}

bool OPCClient::setLifeTime(int opcID, double lifeTime)
{
    Bottle cmd,reply;
    //Do the right lifeTimer operation (we simply remove it if it is -1)
    if (lifeTime == -1)
    {
        cmd.clear();
        cmd.addString("del");

        Bottle &args = cmd.addList();
        Bottle &id = args.addList();
        id.clear();
        id.addString("id");
        id.addInt(opcID);
        Bottle &props = args.addList();
        props.clear();
        props.addString("propSet");
        props.addList().addString("lifeTimer");
    }
    else
    {
        cmd.clear();
        cmd.addString("set");

        Bottle &args = cmd.addList();
        Bottle &id = args.addList();
        id.clear();
        id.addString("id");
        id.addInt(opcID);
        Bottle &props = args.addList();
        props.clear();
        props.addString("lifeTimer");
        props.addDouble(lifeTime);
    }

    write(cmd,reply,isVerbose);
    if (reply.get(0).asVocab() == VOCAB4('n','a','c','k'))
    {
        yError() << "Unable to talk correctly to OPC in setLifeTime";
        yError() << "Command was:" << cmd.toString();
        yError() << "Reply was:" << reply.toString();
        return false;
    }
    return true;
}

list<Relation> OPCClient::getRelations()
{
    list<Relation> relations;

    //Checkout the relations
    Bottle cmd,sub,reply;
    cmd.addString("ask");
    Bottle& cond = cmd.addList();

    sub.addString(ICUBCLIENT_OPC_ENTITY_TAG);
    sub.addString("==");
    sub.addString(ICUBCLIENT_OPC_ENTITY_RELATION);
    cond.addList() = sub;
    write(cmd,reply,isVerbose);

    if (reply.get(0).asVocab() == VOCAB4('n','a','c','k'))
    {
        yError() << "Unable to talk to OPC.";
        return relations;
    }

    //yDebug() << reply.toString();
    Bottle* ids = reply.get(1).asList()->get(1).asList();
    for(int i=0;i<ids->size();i++)
    {
        Bottle getReply;
        int currentID = ids->get(i).asInt();
        cmd.clear();
        cmd.addString("get");
        sub.clear();
        sub.addString("id");
        sub.addInt(currentID);
        cmd.addList().addList() = sub;
        write(cmd,getReply,isVerbose);
        if (getReply.get(0).asVocab() == VOCAB4('n','a','c','k'))
        {
            yError() << "Unable to talk to OPC.";
            return relations;
        }

        Relation r(*getReply.get(1).asList());
        r.m_opcId = currentID;
        relations.push_back(r);
    }
    return relations;
}

std::list<Relation>  OPCClient::getRelationsMatching(std::string subject,std::string verb, std::string object, std::string c_place, std::string c_time, std::string c_manner)
{
    list<Relation> relations;
    //Checkout the relations
    Bottle cmd,sub,reply;
    cmd.addString("ask");
    Bottle& cond = cmd.addList();

    sub.addString(ICUBCLIENT_OPC_ENTITY_TAG);
    sub.addString("==");
    sub.addString(ICUBCLIENT_OPC_ENTITY_RELATION);
    cond.addList() = sub;
    if (subject != "any")
    {
        cond.addString("&&");
        Bottle& subCond = cond.addList();
        subCond.addString("rSubject");
        subCond.addString("==");
        subCond.addString(subject.c_str());
    }
    if (verb != "any")
    {
        cond.addString("&&");
        Bottle& subCond = cond.addList();
        subCond.addString("rVerb");
        subCond.addString("==");
        subCond.addString(verb.c_str());
    }
    if (object != "any")
    {
        cond.addString("&&");
        Bottle& subCond = cond.addList();
        subCond.addString("rObject");
        subCond.addString("==");
        subCond.addString(object.c_str());
    }
    if (c_time != "any")
    {
        cond.addString("&&");
        Bottle& subCond = cond.addList();
        subCond.addString("rCompTime");
        subCond.addString("==");
        subCond.addString(c_time.c_str());
    }
    if (c_place != "any")
    {
        cond.addString("&&");
        Bottle& subCond = cond.addList();
        subCond.addString("rCompPlace");
        subCond.addString("==");
        subCond.addString(c_place.c_str());
    }
    if (c_manner != "any")
    {
        cond.addString("&&");
        Bottle& subCond = cond.addList();
        subCond.addString("rCompManner");
        subCond.addString("==");
        subCond.addString(c_manner.c_str());
    }
    write(cmd,reply,isVerbose);

    if (reply.get(0).asVocab() == VOCAB4('n','a','c','k'))
    {
        yError() << "Unable to talk to OPC.";
        return relations;
    }

    //yDebug() << reply.toString();
    Bottle* ids = reply.get(1).asList()->get(1).asList();
    for(int i=0;i<ids->size();i++)
    {
        Bottle getReply;
        int currentID = ids->get(i).asInt();
        cmd.clear();
        cmd.addString("get");
        sub.clear();
        sub.addString("id");
        sub.addInt(currentID);
        cmd.addList().addList() = sub;
        write(cmd,getReply,isVerbose);
        if (getReply.get(0).asVocab() == VOCAB4('n','a','c','k'))
        {
            yError() << "Unable to talk to OPC.";
            return relations;
        }

        Relation r(*getReply.get(1).asList());
        r.m_opcId = i;
        relations.push_back(r);
    }
    return relations;
}
std::list<Relation>  OPCClient::getRelationsMatchingLoosly(std::string subject, std::string verb, std::string object, std::string c_place, std::string c_time, std::string c_manner)
{
    list<Relation> relations = getRelations();
    list<Relation> filteredRelations;
    for (list<Relation>::iterator it = relations.begin(); it != relations.end(); it++)
    {
        if (it->subject() == subject ||
                it->verb() == verb ||
                it->object() == object ||
                it->complement_time() == c_time ||
                it->complement_place() == c_place ||
                it->complement_manner() == c_manner)
        {
            filteredRelations.push_back(*it);
        }
    }
    return filteredRelations;
}

std::list<Relation>  OPCClient::getRelations(std::string entity)
{
    list<Relation> relations = getRelations();
    list<Relation> filteredRelations;
    for(list<Relation>::iterator it = relations.begin(); it != relations.end() ; it++)
    {
        if (it->subject() == entity ||
                it->verb() == entity ||
                it->object() == entity ||
                it->complement_time() == entity ||
                it->complement_place() == entity ||
                it->complement_manner() == entity)
        {
            filteredRelations.push_back(*it);
        }
    }
    //return filteredRelations;
    ////Checkout the relations
    //Bottle cmd,reply;
    //cmd.addString("ask");


    //Bottle isRelation;
    //isRelation.addString(ICUBCLIENT_OPC_ENTITY_TAG);
    //isRelation.addString("==");
    //isRelation.addString(ICUBCLIENT_OPC_ENTITY_RELATION);

    //Bottle matchSubject;
    //matchSubject.addString("rSubject");
    //matchSubject.addString("==");
    //matchSubject.addString(entity.c_str());

    //Bottle matchObject;
    //matchObject.addString("rObject");
    //matchObject.addString("==");
    //matchObject.addString(entity.c_str());

    //Bottle matchVerb;
    //matchVerb.addString("rVerb");
    //matchVerb.addString("==");
    //matchVerb.addString(entity.c_str());

    //Bottle matchCT;
    //matchCT.addString("rCompTime");
    //matchCT.addString("==");
    //matchCT.addString(entity.c_str());

    //Bottle matchCP;
    //matchCP.addString("rCompPlace");
    //matchCP.addString("==");
    //matchCP.addString(entity.c_str());

    //Bottle matchCM;
    //matchCM.addString("rCompManner");
    //matchCM.addString("==");
    //matchCM.addString(entity.c_str());

    //Bottle& cond1 = cmd.addList();
    //cond1.addList() = isRelation;
    //cond1.addString("&&");
    //cond1.addList() = matchSubject;
    //cmd.addString("||");
    //Bottle& cond2 = cmd.addList();
    //cond2.addList() = isRelation;
    //cond2.addString("&&");
    //cond2.addList() = matchObject;
    //cmd.addString("||");
    //Bottle& cond3 = cmd.addList();
    //cond3.addList() = isRelation;
    //cond3.addString("&&");
    //cond3.addList() = matchVerb;
    //cmd.addString("||");
    //Bottle& cond4 = cmd.addList();
    //cond4.addList() = isRelation;
    //cond4.addString("&&");
    //cond4.addList() = matchCP;
    //cmd.addString("||");
    //Bottle& cond5 = cmd.addList();
    //cond5.addList() = isRelation;
    //cond5.addString("&&");
    //cond5.addList() = matchCT;
    //cmd.addString("||");
    //Bottle& cond6 = cmd.addList();
    //cond6.addList() = isRelation;
    //cond6.addString("&&");
    //cond6.addList() = matchCM;

    //write(cmd,reply,isVerbose);
    //
    //if (reply.get(0).asVocab() == VOCAB4('n','a','c','k'))
    //{
    //    if (isVerbose)
    //        yError() << "Unable to talk to OPC.";
    //    return relations;
    //}

    //yDebug() << reply.toString();
    //Bottle* ids = reply.get(1).asList()->get(1).asList();
    //for(int i=0;i<ids->size();i++)
    //{
    //    Bottle getReply, sub;
    //    int currentID = ids->get(i).asInt();
    //    cmd.clear();
    //    cmd.addString("get");
    //    sub.clear();
    //    sub.addString("id");
    //    sub.addInt(currentID);
    //    cmd.addList().addList() = sub;
    //    write(cmd,getReply,isVerbose);
    //    if (getReply.get(0).asVocab() == VOCAB4('n','a','c','k'))
    //    {
    //        if(this->isVerbose)
    //            yError() << "Unable to talk to OPC.";
    //        return relations;
    //    }

    //    Relation r(*getReply.get(1).asList());
    //    r.m_opcId = i;
    //    relations.push_back(r);
    //}
    return filteredRelations;
}

std::list<Relation>  OPCClient::getRelations(Entity* entity)
{
    return getRelations(entity->name());
}

//Clear local dictionnaries and populate them using the server content
void OPCClient::checkout(bool updateCache)
{
    if (updateCache)
    {
        //Clear the content of dictionaries
        for(map<int,Entity*>::iterator it=entitiesByID.begin(); it!=entitiesByID.end(); it++)
        {
            delete it->second;
        }
        entitiesByID.clear();
    }

    //Find the ids of all entities within the OPC
    Bottle cmd,sub,reply;
    cmd.addString("ask");

    Bottle& cond = cmd.addList();
    sub.addString(ICUBCLIENT_OPC_ENTITY_TAG);
    sub.addString("!=");
    sub.addString(ICUBCLIENT_OPC_ENTITY_RELATION);
    cond.addList() = sub;

    write(cmd,reply,isVerbose);
    if (reply.get(0).asVocab() == VOCAB4('n','a','c','k'))
    {
        yError() << "Unable to talk to OPC.";
        return;
    }

    Bottle* ids = reply.get(1).asList()->get(1).asList();
    for(int i=0;i<ids->size();i++)
    {
        int currentID = ids->get(i).asInt();
        getEntity(currentID, updateCache); //Automatically update the dictionnaries
    }
    if (isVerbose)
        yInfo() << "Checkout result: "<< ids->size() << " entities added.";
}

//Update the whole content of the world (poll the OPC for all items)
void OPCClient::update()
{
    for(map<int, Entity* >::iterator it = entitiesByID.begin(); it != entitiesByID.end(); it++)
    {
        update(it->second);
    }
}

//Update a given entity
void OPCClient::update(Entity *e)
{
    //Try to retrieve the object based on its OPC ID
    Bottle cmd,reply;
    cmd.addVocab(Vocab::encode("get"));
    Bottle& query = cmd.addList();
    Bottle& id = query.addList();
    id.addString("id");
    id.addInt(e->opc_id());
//    yDebug() << "OPCClient send request to OPC: " << cmd.toString();
    write(cmd,reply,isVerbose);
//    yDebug() << "OPCClient response: " << reply.toString();

    if (reply.get(0).asVocab() == VOCAB4('n','a','c','k'))
    {
        yError() << "OPC Client: error while updating " << e->opc_id();
        return;
    }

    //Fill the datastructure with the bottle content
    Bottle props = *reply.get(1).asList();
    if(!e->fromBottle(props)) {
        yError() << "Error updating entity " << e->name() << "fromBottle! Type: " << e->entity_type();
        yDebug() << "OPCClient props:" << props.toString();
    }
//    yDebug() << "OPCClient fromBottle success";

    //Set the initial signature of this entity
    e->m_original_entity = e->asBottle();
}

//Commit the whole world to the OPC (erase everything)
void OPCClient::commit()
{
    for(map<int, Entity* >::iterator it = entitiesByID.begin(); it != entitiesByID.end(); it++)
    {
        commit(it->second);
    }
}

//Commit a single entity to the OPC (erase it)
void OPCClient::commit(Entity *e)
{
    Bottle cmd,reply;
    cmd.addVocab(Vocab::encode("set"));
    Bottle& query = cmd.addList();
    Bottle& id = query.addList();
    id.addString("id");
    id.addInt(e->opc_id());

    Bottle props=e->asBottleOnlyModifiedProperties();
    for (int i=0; i<props.size(); i++)
        query.addList()=*props.get(i).asList();

    write(cmd,reply,isVerbose);
    if (reply.get(0).asVocab()==VOCAB4('n','a','c','k'))
    {
        yError() << "OPC Client: error while commiting " << e->opc_id();
        return;
    }
}

list<Entity*> OPCClient::Entities(const Bottle &condition)
{
    list<Entity*> matchingEntities;
    //Find the ids of all entities within the OPC
    Bottle cmd,reply;
    cmd.addString("ask");
    cmd.addList() = condition;

    write(cmd,reply,isVerbose);

    if (reply.get(0).asVocab() == VOCAB4('n','a','c','k'))
    {
        yError() << "Unable to talk to OPC.";
        return matchingEntities;
    }

    Bottle* ids = reply.get(1).asList()->get(1).asList();
    for(int i=0;i<ids->size();i++)
    {
        int currentID = ids->get(i).asInt();
        matchingEntities.push_back(getEntity(currentID)); //Automatically update the dictionnaries
        update(matchingEntities.back());
    }
    if (isVerbose)
        yInfo() << "Checkout result: " << ids->size() << " entities added.";
    return matchingEntities;
}

list<Entity*> OPCClient::Entities(const string &prop, const string &op, const string &value)
{
    //Find the ids of all entities within the OPC
    Bottle cond,sub;
    sub.addString(prop.c_str());
    sub.addString(op.c_str());
    sub.addString(value.c_str());
    cond.addList() = sub;
    return Entities(cond);
}

//Getter of the list of entities stored locally
list<Entity*> OPCClient::EntitiesCache()
{
    list<Entity*> lR;
    for(map<int,Entity*>::iterator it = this->entitiesByID.begin() ; it != this->entitiesByID.end() ; it++)
    {
        lR.push_back(it->second);
    }
    return lR;
}


//Getter of the list of entities stored locally
std::list<std::shared_ptr<Entity>> OPCClient::EntitiesCacheCopy()
{
    list<std::shared_ptr<Entity>> lR;
    for(map<int,Entity*>::iterator it = this->entitiesByID.begin() ; it != this->entitiesByID.end() ; it++)
    {
        std::shared_ptr<Entity> E;
        if ((it->second)->m_entity_type == ICUBCLIENT_OPC_ENTITY_AGENT)
        {
            E = std::shared_ptr<Agent>(new Agent());
        }
        else if ((it->second)->m_entity_type == ICUBCLIENT_OPC_ENTITY_OBJECT)
        {
            E = std::shared_ptr<Object>(new Object());
        }
        else if ((it->second)->m_entity_type == ICUBCLIENT_OPC_ENTITY_ACTION)
        {
            E = std::shared_ptr<Action>(new Action());
        }
        else if ((it->second)->m_entity_type == "bodypart")
        {
            E = std::shared_ptr<Bodypart>(new Bodypart());
        }
        else
        {
            yError() << "EntitiesCacheCopy: Unknown entity type!";
        }

        if(E != NULL) {
            E->fromBottle(it->second->asBottle());
            E->m_opc_id = it->second->m_opc_id;
            lR.push_back(E);
        }
    }
    return lR;
}

//Return a (printable) string with the contents of the OPC
string OPCClient::print()
{
    string s="WORLD STATE \n";
    for(map<int, Entity* >::iterator it = entitiesByID.begin(); it != entitiesByID.end(); it++)
    {
        s += it->second->toString();
    }
    s+= "RELATIONS \n";
    list<Relation> rels = getRelations();
    for(list<Relation>::iterator it = rels.begin(); it!=rels.end(); it++)
        s += it->toString() + '\n';

    return s;
}

bool OPCClient::changeName(Entity *e, const std::string &newName)
{
    for(auto& entity : entitiesByID)
    {
        if(entity.second->name() == newName) {
            yError() << "Entity with name " << newName << " is already existing.";
            return false;
        }
    }
    e->changeName(newName);
    return true;
}
