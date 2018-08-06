/* 
 * Copyright (C) 2014 WYSIWYD Consortium, European Commission FP7 Project ICT-612139
 * Authors: Stéphane Lallée
 * email:   stephane.lallee@gmail.com
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

#include <iostream>
#include <fstream>
#include <iomanip>
#include <map>
#include <string>

#include <yarp/os/all.h>
#include <yarp/sig/all.h>
#include <yarp/math/Math.h>

#include <iCub/ctrl/math.h>
#include <iCub/optimization/calibReference.h>
#include "icubclient/tags.h"
#include "icubclient/functions.h"

using namespace std;
using namespace yarp::os;
using namespace yarp::sig;
using namespace yarp::math;
using namespace iCub::ctrl;
using namespace iCub::optimization;
using namespace icubclient;

/**
 * \ingroup referenceFrameHandler
 */
struct FrameInfo
{
    string name;
    bool isCalibrated;
    Matrix H,HInv,S,SInv;
    double calibrationError;
    CalibReferenceWithMatchedPoints calibrator;
};

/**
 * \ingroup referenceFrameHandler
 */
class FrameHandlerModule: public RFModule
{
    //Store the translation matrices allowing transformation from one frame toward the icub pivot frame
    map<string, FrameInfo> frames;
    string matricesFilePath; //!< This points to the home context path (.local)
    bool isVerbose;
    RpcServer rpc;
    RpcClient opc; //!< Directly talk to OPC, do not use OPCClient here
    Vector bboxMin,bboxMax;
    Vector scaleMin,scaleMax;

public:
    /************************************************************************/
    bool configure(ResourceFinder &rf)
    {
        setName( rf.check("name",Value("referenceFrameHandler")).asString().c_str() );
        isVerbose = (rf.check("isVerbose",Value(0)).asInt() == 1);
        bool isEmpty = rf.check("empty")||rf.check("empty");

        //Define the working bounding box
        bboxMin.resize(3); bboxMax.resize(3);
        bboxMin[0]=-1.0;   bboxMax[0]=1.0;
        bboxMin[1]=-1.0;   bboxMax[1]=1.0;
        bboxMin[2]=-1.0;   bboxMax[2]=1.0;

        //Define the scaling bounding box
        scaleMin.resize(3); scaleMax.resize(3);
        scaleMin[0]=0.1;    scaleMax[0]=10.0;
        scaleMin[1]=0.1;    scaleMax[1]=10.0;
        scaleMin[2]=1.0;    scaleMax[2]=1.0;

        //Create the pivot reference
        frames["icub"].name = "icub";
        frames["icub"].isCalibrated = true;
        frames["icub"].H = eye(4,4);
        frames["icub"].S = eye(4,4);
        frames["icub"].HInv = frames["icub"].H;
        frames["icub"].SInv = frames["icub"].S;

        //Load matrices
        string matricesFileName = rf.check("matricesFile",Value("frames.ini")).asString();
        matricesFilePath = rf.findFile(matricesFileName);
        Property matricesProp; matricesProp.fromConfigFile(matricesFilePath);
        if (!isEmpty)
            LoadMatrices(matricesProp);

        // we store new matrices in the home context path instead
        matricesFilePath=rf.getHomeContextPath()+"/"+matricesFileName;

        opc.open("/" + getName() + "/opc:rpc"); //Create port to OPC
        rpc.open("/" + getName() + "/rpc"); //Create RPC port
        attach(rpc); 

        return true;
    }

    /**
     * @brief Inverts the scale of the provided matrix
     * @param in - input matrix
     * @return Matrix with inverted scale
     */
    Matrix inverseScale(const Matrix &in)
    {
        Matrix out = in;
        out(0,0) = 1.0 / out(0,0);
        out(1,1) = 1.0 / out(1,1);
        out(2,2) = 1.0 / out(2,2);
        return out;
    }

    /**
     * @brief LoadMatrices - reads the matrices from the specified *matricesFile* file. Use `SaveMatrices` to write this file in the proper format.
     */
    void LoadMatrices(Property &prop)
    {
        Bottle &bFrames = prop.findGroup("frames");
        for(unsigned int f=1; f<bFrames.size(); f+=3)
        {
            string currentFrame = bFrames.get(f).toString().c_str();
            Bottle *bH = bFrames.get(f+1).asList();
            Matrix H(4,4);
            for(int i=0;i<4;i++)
            {
                for(int j=0;j<4;j++)
                {
                    H(i,j)=bH->get(4*i+j).asDouble();
                }
            }

            Bottle *bS = bFrames.get(f+2).asList();
            Matrix S(4,4);
            for(int i=0;i<4;i++)
            {
                for(int j=0;j<4;j++)
                {
                    S(i,j)=bS->get(4*i+j).asDouble();
                }
            }

            frames[currentFrame].name = currentFrame;
            frames[currentFrame].H = H;
            frames[currentFrame].HInv = SE3inv(frames[currentFrame].H);
            frames[currentFrame].S = S;
            frames[currentFrame].SInv = inverseScale(frames[currentFrame].S);
            frames[currentFrame].isCalibrated = true;

            cout<<"Loading Frame"<<endl
                <<frames[currentFrame].name<<endl
                <<frames[currentFrame].H.toString(3,3)<<endl
                <<frames[currentFrame].S.toString(3,3)<<endl;
        }
    }


    /**
     * @brief Saves the matrices to a file; this file can later be loaded using `LoadMatrices`
     * @param fileName - absolute path to the file
     */
    void SaveMatrices(const string &fileName)
    {
        ofstream file(fileName.c_str());
        file<<"[frames]"<<endl;
        for(map<string, FrameInfo>::iterator it=frames.begin(); it!= frames.end(); it++)
        {
            if (it->second.isCalibrated)
            {
                file<<it->second.name<<endl;
                Bottle b;
                b.clear();

                for(int i=0;i<4;i++)
                {
                    for(int j=0;j<4;j++)
                    {
                        b.addDouble(it->second.H(i,j));
                    }
                }

                file<< b.toString().c_str()<<endl;

                Bottle bScale;
                bScale.clear();
                for(int i=0;i<4;i++)
                {
                    for(int j=0;j<4;j++)
                    {
                        bScale.addDouble(it->second.S(i,j));
                    }
                }
                file<< bScale.toString().c_str()<<endl;

                cout<<"Saving Frame"<<endl
                    <<it->second.name<<endl
                    <<it->second.H.toString(3,3)<<endl
                    <<"Scale"<<endl
                    <<it->second.S.toString(3,3)<<endl;
            }
        }
        file.close();
    }

    /**
     * @brief Loads the calibration matrices from the OPC
     * \deprecated This is not supported anymore and may be removed in future versions
     */
    void LoadMatricesFromOPC()
    {
        Bottle opcCmd,opcReply;
        opcCmd.addVocab(Vocab::encode("ask"));
        Bottle &query=opcCmd.addList();
        query.addList().addString(ICUBCLIENT_OPC_FRAME_NAME);
        opc.write(opcCmd,opcReply);

        Bottle ids=opcGetIdsFromAsk(opcReply);

        for(unsigned int i=0; i<ids.size() ; i++)
        {
            opcCmd.clear();
            opcCmd.addVocab(Vocab::encode("get"));
            Bottle &query=opcCmd.addList();
            Bottle &idProp=query.addList();
            idProp.addString("id");
            idProp.addInt(ids.get(i).asInt());
            Bottle &propSet=query.addList();
            propSet.addString("propSet");
            Bottle &props=propSet.addList();
            props.addString(ICUBCLIENT_OPC_FRAME_NAME);
            props.addString(ICUBCLIENT_OPC_FRAME_MATRIX);
            props.addString(ICUBCLIENT_OPC_FRAME_SCALE);
            
            Bottle reply;
            reply.clear();
            opc.write(opcCmd,reply);

            if (reply.get(0).asVocab()==Vocab::encode("ack"))
            {
                if (Bottle *propList=reply.get(1).asList())
                {
                    string currentFrame = propList->find(ICUBCLIENT_OPC_FRAME_NAME).asString().c_str();
                    Bottle *bH = propList->find(ICUBCLIENT_OPC_FRAME_MATRIX).asList();
                    Matrix H(4,4);
                    for(int i=0;i<4;i++)
                    {
                        for(int j=0;j<4;j++)
                        {
                            H(i,j)=bH->get(4*i+j).asDouble();
                        }
                    }
                                
                    Bottle *bS = propList->find(ICUBCLIENT_OPC_FRAME_SCALE).asList();
                    Matrix S(4,4);
                    for(int i=0;i<4;i++)
                    {
                        for(int j=0;j<4;j++)
                        {
                            S(i,j)=bS->get(4*i+j).asDouble();
                        }
                    }

                    frames[currentFrame].name = currentFrame;
                    frames[currentFrame].H = H;
                    frames[currentFrame].HInv = SE3inv(frames[currentFrame].H);
                    frames[currentFrame].S = S;
                    frames[currentFrame].SInv = inverseScale(frames[currentFrame].S);
                    frames[currentFrame].isCalibrated = true;

                    cout<<"Reading Frame from OPC"<<endl
                        <<frames[currentFrame].name<<endl
                        <<frames[currentFrame].H.toString(3,3)<<endl
                        <<frames[currentFrame].S.toString(3,3)<<endl;
                }
            }
        }
    }

    /**
     * @brief Saves the calibration matrices to the OPC
     * \deprecated This is not supported anymore and may be removed in future versions
     */
    void SaveMatrices2OPC()
    {
        for(map<string, FrameInfo>::iterator it=frames.begin(); it!= frames.end(); it++)
        {
            if (it->second.isCalibrated)
            {

                Bottle opcCmd,opcReply;
                opcCmd.addVocab(Vocab::encode("ask"));
                Bottle &query=opcCmd.addList();
                Bottle& checkName = query.addList();
                checkName.addString(ICUBCLIENT_OPC_FRAME_NAME);
                checkName.addString("==");
                checkName.addString(it->second.name.c_str());

                Bottle ids=opcGetIdsFromAsk(opcReply);

                opcCmd.clear();

                if (ids.size() == 0)
                    opcCmd.addVocab(Vocab::encode("add"));
                else
                    opcCmd.addVocab(Vocab::encode("set"));

                Bottle &content = opcCmd.addList();
                if (ids.size() != 0 )
                {
                    Bottle &idUp = content.addList();
                    idUp.addString("id");
                    idUp.addInt(ids.get(0).asInt());
                }

                Bottle &name = content.addList();
                name.addString(ICUBCLIENT_OPC_FRAME_NAME);
                name.addString(it->second.name.c_str());
                                
                Bottle &matrix = content.addList();
                matrix.addString(ICUBCLIENT_OPC_FRAME_MATRIX);
                Bottle &bMat = matrix.addList();

                for(int i=0;i<4;i++)
                {
                    for(int j=0;j<4;j++)
                    {
                        bMat.addDouble(it->second.H(i,j));
                    }
                }

                Bottle &scale = content.addList();
                scale.addString(ICUBCLIENT_OPC_FRAME_SCALE);
                Bottle &bSca = matrix.addList();
                for(int i=0;i<4;i++)
                {
                    for(int j=0;j<4;j++)
                    {
                        bSca.addDouble(it->second.S(i,j));
                    }
                }

                Bottle lastReply;
                lastReply.clear();
                opc.write(opcCmd,lastReply);
                cout<<"Writing Frame 2 OPC"<<endl
                    <<it->second.name<<endl
                    <<it->second.H.toString(3,3)<<endl
                    <<"Scale"<<endl
                    <<it->second.S.toString(3,3)<<endl
                    <<lastReply.toString().c_str()<<endl;
            }
        }
    }

    /************************************************************************/
    bool respond(const Bottle& cmd, Bottle& reply) 
    {
        //On the basis of the command sent an action is performed
        switch (cmd.get(0).asVocab())
        {
            //Write matrices to opc - not supported!
            case yarp::os::createVocab('o','p','c','w'):
            {
                SaveMatrices2OPC();
                reply.addString("ack");
                reply.addString("written to OPC");
                reply.addString("this method is deprecated!");
                break;
            }

            //read matrices from opc - not supported!
            case yarp::os::createVocab('o','p','c','r'):
            {
                LoadMatricesFromOPC();
                reply.addString("ack");
                reply.addString("updated from OPC");
                reply.addString("this method is deprecated!");
                break;
            }

            //save matrices to file
            case yarp::os::createVocab('s','a','v','e'):
            {
                SaveMatrices(matricesFilePath.c_str());
                reply.addString("ack");
                reply.addString("saved");
                reply.addString( matricesFilePath.c_str() );
                break;
            }
                    
            //Clear the list of points used for calibration
            case yarp::os::createVocab('c','l','e','a'):
            {
                //Format is [clear] <frameName>
                string frameName = cmd.get(1).asString().c_str();
                std::cout<<"Clearing the points for frame : "<<frameName<<std::endl;
                std::cout<<"Erasing "<<frames[frameName].calibrator.getNumPoints()<<std::endl;
                //If the frame is not created yet we create it
                if (frames.find(frameName) == frames.end() )
                {
                    std::cout<<"Frame "<< frameName<<" was not present, creating it."<<std::endl;
                    frames[frameName].name = frameName;
                    frames[frameName].isCalibrated = false;
                    frames[frameName].calibrator.setBounds(bboxMin,bboxMax);
                    frames[frameName].calibrator.setScalingBounds(scaleMin,scaleMax);
                    frames[frameName].S = eye(4,4);
                    frames[frameName].H = eye(4,4);
                    frames[frameName].SInv = eye(4,4);
                    frames[frameName].HInv = eye(4,4);
                }
      
                frames[frameName].calibrator.clearPoints();
                
                std::cout<<"After "<<frames[frameName].calibrator.getNumPoints()<<std::endl;
                reply.addString("ack");
                reply.addString("Cleared");
                break;
            }        

            //Add a point to the list of points used for calibration
            case yarp::os::createVocab('a','d','d'):
            {
                //Format is [add] <frameName> (x y z) (x' y' z')
                string frameName = cmd.get(1).asString().c_str();

                //If the frame is not created yet we create it
                if (frames.find(frameName) == frames.end() )
                {
                    frames[frameName].name = frameName;
                    frames[frameName].isCalibrated = false;
                    frames[frameName].calibrator.setBounds(bboxMin,bboxMax);
                    frames[frameName].calibrator.setScalingBounds(scaleMin,scaleMax);
                    frames[frameName].S = eye(4,4);
                    frames[frameName].H = eye(4,4);
                    frames[frameName].SInv = eye(4,4);
                    frames[frameName].HInv = eye(4,4);
                }

                //Add the point
                Vector pOtherFrame(3), pPivotFrame(3);
                for(int i=0; i<3; i++)
                {
                    pOtherFrame[i] = cmd.get(2).asList()->get(i).asDouble();
                    pPivotFrame[i] = cmd.get(3).asList()->get(i).asDouble();
                }
                frames[frameName].calibrator.addPoints(pOtherFrame,pPivotFrame);
                reply.addString("ack");
                reply.addString("Point added");
                break;
            }

            //Calibrate a specific frame
            case yarp::os::createVocab('c','a','l'):
            {
                string frameName = cmd.get(1).asString().c_str();

                //If the frame is not created yet it is an error
                if (frames.find(frameName) == frames.end() )
                {
                    reply.addString("nack");
                    reply.addString("Reference frame does not exist.");
                    break;
                }
                else
                {
                    double t0=Time::now();
                    frames[frameName].calibrator.calibrate(frames[frameName].H,frames[frameName].calibrationError);
                    double dt=Time::now()-t0;
                    frames[frameName].HInv = SE3inv(frames[frameName].H);
                    frames[frameName].S=eye(4,4);
                    frames[frameName].SInv = inverseScale(frames[frameName].S);
                    

                    cout<<"Asked for calibration of "<<frameName<<endl;
                    cout<<"H"<<endl<<frames[frameName].H.toString(3,3).c_str()<<endl;
                    cout<<"H^-1"<<endl<<frames[frameName].HInv.toString(3,3).c_str()<<endl;
                    cout<<"residual = "<<frames[frameName].calibrationError<<" [m]"<<endl;
                    cout<<"calibration performed in "<<dt<<" [s]"<<endl;
                    cout<<endl;
                    frames[frameName].isCalibrated = true;

                    reply.addString("ack");
                    reply.addString(frames[frameName].H.toString());
                }
                break;
            }

            //Calibrate a specific frame with scaling option
            case yarp::os::createVocab('s','c','a','l'):
            {
                string frameName = cmd.get(1).asString().c_str();

                //If the frame is not created yet it is an error
                if (frames.find(frameName) == frames.end() )
                {
                    reply.addString("nack");
                    reply.addString("Reference frame does not exist.");
                    break;
                }
                else
                {
                    Vector vScale(3);
                    double t0=Time::now();
                    frames[frameName].calibrator.calibrate(frames[frameName].H, vScale, frames[frameName].calibrationError);
                    double dt=Time::now()-t0;
                    frames[frameName].S = eye(4,4);
                    frames[frameName].S(0,0) = vScale(0);
                    frames[frameName].S(1,1) = vScale(1);
                    frames[frameName].S(2,2) = vScale(2);

                    frames[frameName].HInv = SE3inv(frames[frameName].H);
                    frames[frameName].SInv = inverseScale(frames[frameName].S);

                    cout<<"Asked for calibration of "<<frameName<<endl;
                    cout<<"H"<<endl<<frames[frameName].H.toString(3,3).c_str()<<endl;
                    cout<<"H^-1"<<endl<<frames[frameName].HInv.toString(3,3).c_str()<<endl;
                    cout<<"S"<<endl<<frames[frameName].S.toString(3,3).c_str()<<endl;
                    cout<<"S^-1"<<endl<<frames[frameName].SInv.toString(3,3).c_str()<<endl;
                    cout<<"residual = "<<frames[frameName].calibrationError<<" [m]"<<endl;
                    cout<<"calibration performed in "<<dt<<" [s]"<<endl;
                    cout<<endl;
                    frames[frameName].isCalibrated = true;
                    reply.addString("ack");
                    reply.addString(frames[frameName].H.toString());
                }
                break;
            }

            //Calibrate a specific frame with isotropic scaling option
            case yarp::os::createVocab('i','c','a','l'):
            {
                string frameName = cmd.get(1).asString().c_str();

                //If the frame is not created yet it is an error
                if (frames.find(frameName) == frames.end() )
                {
                    reply.addString("nack");
                    reply.addString("Reference frame does not exist.");
                    break;
                }
                else
                {
                    double vScale;
                    double t0=Time::now();
                    frames[frameName].calibrator.calibrate(frames[frameName].H, vScale, frames[frameName].calibrationError);
                    double dt=Time::now()-t0;
                    frames[frameName].S = eye(4,4);
                    frames[frameName].S(0,0) = vScale;
                    frames[frameName].S(1,1) = vScale;
                    frames[frameName].S(2,2) = vScale;

                    frames[frameName].HInv = SE3inv(frames[frameName].H);
                    frames[frameName].SInv = inverseScale(frames[frameName].S);

                    cout<<"Asked for calibration of "<<frameName<<endl;
                    cout<<"H"<<endl<<frames[frameName].H.toString(3,3).c_str()<<endl;
                    cout<<"H^-1"<<endl<<frames[frameName].HInv.toString(3,3).c_str()<<endl;
                    cout<<"S"<<endl<<frames[frameName].S.toString(3,3).c_str()<<endl;
                    cout<<"S^-1"<<endl<<frames[frameName].SInv.toString(3,3).c_str()<<endl;
                    cout<<"residual = "<<frames[frameName].calibrationError<<" [m]"<<endl;
                    cout<<"calibration performed in "<<dt<<" [s]"<<endl;
                    cout<<endl;
                    frames[frameName].isCalibrated = true;

                    reply.addString("ack");
                    reply.addString(frames[frameName].H.toString());
                }
                break;
            }
            //Transform one point from a specific reference frame to the pivot frame (icub)
            case yarp::os::createVocab('t','r','a','n'):
            {
                string frameName1 = cmd.get(1).asString().c_str();
                string frameName2 = cmd.get(2).asString().c_str();

                //If one of the frames is not created yet it is an error
                if (frames.find(frameName1) == frames.end() || frames.find(frameName2) == frames.end() )
                {
                    reply.addString("nack");
                    reply.addString("Reference frame does not exist.");
                    break;
                }

                //If one of the frames is not calibrated yet it is an error
                if (!frames[frameName1].isCalibrated || !frames[frameName2].isCalibrated )
                {
                    reply.addString("nack");
                    reply.addString("Reference frame is not calibrated.");
                    break;
                }

                Vector p0(4),p1(4),p2(4);
                for(int i=0;i<3;i++)
                    p0[i] = cmd.get(3).asList()->get(i).asDouble();
                p0[3] = 1;

                //Convert from frame1 to the pivot frame (iCub)
                Matrix H = frames[frameName1].H;
                Matrix S = frames[frameName1].S;
                p1 = S*H*p0;

                //Convert from pivot (iCub) to the frame2
                H = frames[frameName2].HInv;
                S = frames[frameName2].SInv;
                p2 = S*H*p1;

                if (isVerbose)
                {
                    cout<<"Point in "<<frameName1<<" frame"<<p0.toString()<<endl;
                    cout<<"Point in iCub frame"<<p1.toString()<<endl;
                    cout<<"Point in "<<frameName2<<" frame"<<p2.toString()<<endl;
                }
                reply.addString("ack");
                Bottle &bVect = reply.addList();
                bVect.addDouble(p2[0]);bVect.addDouble(p2[1]);bVect.addDouble(p2[2]);
                break;
            }
            //Set the transformation matrix from one frame to another, scaling is identity
            case yarp::os::createVocab('s','m','a','t'):
            {                
                string frameName1 = cmd.get(1).asString().c_str();
                string frameName2 = cmd.get(2).asString().c_str();
                cout<<"Received set matrix "<<frameName1<<" => "<<frameName2<<endl;

                if (frameName2 != "icub")
                {
                    reply.addString("nack");
                    reply.addString("Can only set matrices to the icub pivot frame");
                    break;
                }
                frames[frameName1].name = frameName1;
                frames[frameName1].isCalibrated = true;
                frames[frameName1].H = eye(4,4);
                frames[frameName1].S = eye(4,4);
                Bottle *matrixList = cmd.get(3).asList();

                for(int i=0; i<4; i++)
                {
                    for(int j=0; j<4; j++)
                    {
                        frames[frameName1].H(i,j) = matrixList->get(4*j+i).asDouble();
                    }
                }
                frames[frameName1].HInv = SE3inv(frames[frameName1].H);
                frames[frameName1].SInv = inverseScale(frames[frameName1].S);
                reply.addString("ack");

                cout<<"H"<<endl<<frames[frameName1].H.toString(3,3).c_str()<<endl;
                cout<<"H^-1"<<endl<<frames[frameName1].HInv.toString(3,3).c_str()<<endl;
                break;
            }

            //Return the transformation matrix from one frame to another, incorporating scaling information
            case yarp::os::createVocab('m','a','t'):
            {
                string frameName1 = cmd.get(1).asString().c_str();
                string frameName2 = cmd.get(2).asString().c_str();

                //If one of the frames is not created yet it is an error
                if (frames.find(frameName1) == frames.end() || frames.find(frameName2) == frames.end() )
                {
                    reply.addString("nack");
                    reply.addString("Reference frame does not exist.");
                    break;
                }

                //If one of the frames is not calibrated yet it is an error
                if (!frames[frameName1].isCalibrated || !frames[frameName2].isCalibrated )
                {
                    reply.addString("nack");
                    reply.addString("Reference frame is not calibrated.");
                    break;
                }

                reply.addString("ack");
                Matrix h1h2 = frames[frameName1].S*frames[frameName1].H * frames[frameName2].HInv*frames[frameName2].SInv;
                std::cout<<std::endl<<std::endl;
                std::cout<<frameName1<<" H : "<<frames[frameName1].H.toString(3,3)<<std::endl;
                std::cout<<frameName1<<" S : "<<frames[frameName1].S.toString(3,3)<<std::endl;
                std::cout<<frameName2<<" Hinv : "<<frames[frameName2].HInv.toString(3,3)<<std::endl;
                std::cout<<frameName2<<" Sinv : "<<frames[frameName2].SInv.toString(3,3)<<std::endl;    
                std::cout<<"Result H : "<<h1h2.toString(3,3)<<std::endl;    
                     
                Bottle &matrixList = reply.addList();
                for(int i=0; i<4; i++)
                {
                    for(int j=0; j<4; j++)
                    {
                        matrixList.addDouble(h1h2(i,j));
                    }
                }
                
                if (isVerbose)
                {
                    cout<<"Retrieved H for "<<frameName1<<"-->"<<frameName2<<endl
                        <<h1h2.toString(3,3)<<endl
                        <<"Scale"<<endl;
                }

                break;
            }

            default: { reply.addString(getName()+" : unknown command."); return false; }
        }

        return true;
    }

    /************************************************************************/
    bool interruptModule()
    {
        opc.interrupt();
        rpc.interrupt();
        return true;
    }

    /************************************************************************/
    bool close()
    {
        opc.interrupt();
        opc.close();
        rpc.interrupt();
        rpc.close();
        return true;
    }

    /************************************************************************/
    bool updateModule()
    {
        return true;
    }

    /************************************************************************/
    double getPeriod()
    {
        return 0.5;
    }
};

int main(int argc, char *argv[])
{
    Network yarp;

    if (!yarp.checkNetwork())
        return -1;

    ResourceFinder rf;
    rf.setVerbose(true);
    rf.setDefaultContext("referenceFrameHandler");
    rf.setDefaultConfigFile("config.ini");
    rf.configure(argc,argv);
    FrameHandlerModule mod;

    return mod.runModule(rf);
}
