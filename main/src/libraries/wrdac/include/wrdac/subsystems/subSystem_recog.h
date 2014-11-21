/* 
* Copyright (C) 2014 WYSIWYD Consortium, European Commission FP7 Project ICT-612139
* Authors: Gr�goire Pointeau
* email:   gregoire.pointeau@inserm.fr
* website: http://efaa.upf.edu/ 
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

#ifndef __EFAA_SUBSYSTEM_RECOG_H__
#define __EFAA_SUBSYSTEM_RECOG_H__

#define SUBSYSTEM_RECOG        "recog"

#include "wrdac/subsystems/subSystem.h"
#include <iostream>

namespace wysiwyd{namespace wrdac{

    /**
    * \ingroup wrdac_clients
    *
    * Abstract subSystem for speech recognizer 
    */
    class SubSystem_Recog: public SubSystem
    {
    protected:
        virtual bool connect() { return yarp::os::Network::connect(portRPC.getName(), "/speechRecognizer/rpc"); }

    public:

        yarp::os::Port portRPC;
        SubSystem_Recog(const std::string &masterName):SubSystem(masterName){
            portRPC.open(("/"+m_masterName+"/recog:rpc").c_str());
            m_type = SUBSYSTEM_ABM;
        }


        virtual void Close() {portRPC.interrupt();portRPC.close();};


        /**
        * From one grxml grammar, return the sentence recognized for one timeout
        *
        */
        yarp::os::Bottle recogFromGrammar(std::string &sInput)
        {
            yarp::os::Bottle bMessenger;
            yarp::os::Bottle bReply;
            bMessenger.addString("recog");
            bMessenger.addString("grammarXML");
            bMessenger.addString(sInput);
            portRPC.write(bMessenger, bReply);
            return bReply;
        }


        /**
        *   From one grxml grammar, return the first sentence non-empty recognized
        *   can last for several timeout (by default 50
        */
        yarp::os::Bottle recogFromGrammarLoop(std::string &sInput, const int &iLoop = 50)
        {
            std::ostringstream osError;          
            bool fGetaReply = false;
            yarp::os::Bottle bMessenger, //to be send TO speech recog
                bAnswer,
                bReply, //response from speech recog without transfer information, including raw sentence
                bOutput; // semantic information of the content of the recognition

            bMessenger.addString("recog");
            bMessenger.addString("grammarXML");
            bMessenger.addString(sInput);

            while (!fGetaReply)
            {
                portRPC.write(bMessenger,bReply);

                std::cout << "Reply from Speech Recog : " << bReply.toString() << std::endl;

                if (bReply.toString() == "NACK" || bReply.size() != 2)
                {
                    bOutput.addInt(0);
                    osError << "Check grammar";
                    bOutput.addString(osError.str());
                    std::cout << osError.str() << std::endl;
                    return bOutput;
                }

                if (bReply.get(0).toString() == "0")
                {
                    bOutput.addInt(0);
                    osError << "Grammar not recognized";
                    bOutput.addString(osError.str());
                    std::cout << osError.str() << std::endl;
                    return bOutput;
                }

                bAnswer = *bReply.get(1).asList();

                if (bAnswer.toString() != "" && !bAnswer.isNull())
                {
                    fGetaReply = true;
                    bOutput.addInt(1);
                    bOutput.addList() = bAnswer;
                }
            }

            return bOutput;
        }

    };


}}//Namespace
#endif


