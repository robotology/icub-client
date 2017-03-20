// -*- mode:C++; tab-width:4; c-basic-offset:4; indent-tabs-mode:nil -*-

/*
* Copyright (C) 2014 WYSIWYD Consortium, European Commission FP7 Project ICT-612139
* Authors: Hyung Jin Chang
* email:   hj.chang@imperial.ac.uk
* website: http://wysiwyd.upf.edu/
* Permission is granted to copy, distribute, and/or modify this program
* under the terms of the GNU General Public License, version 2 or any
* later version published by the Free Software Foundation.
*
* A copy of the license can be found at
* $WYSIWYD_ROOT/license/gpl.txt
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
* Public License for more details
*/

#include "faceTracker.h"

using namespace yarp::os;
using namespace yarp::sig;
using namespace yarp::sig::draw;
using namespace yarp::sig::file;
using namespace yarp::dev;
using namespace std;

/************************************************************************/
bool faceTrackerModule::configure(yarp::os::ResourceFinder &rf) {

    moduleName = rf.check("name",
                        Value("faceTracker"),
                        "module name (string)").asString();

    setName(moduleName.c_str());

    opcName = rf.check("opcName",
                        Value("OPC"),
                        "Opc name (string)").asString();

    string xmlPath = rf.findFileByName("haarcascade_frontalface_default.xml");

    // Create an iCub Client and check that all dependencies are here befor starting
    opc = new OPCClient(moduleName.c_str());
    opc->connect(opcName);
    icub = NULL;

    handlerPortName = "/";
    handlerPortName +=  getName() + "/rpc";

    if (!handlerPort.open(handlerPortName.c_str())) {
        cout << getName() << ": Unable to open port " << handlerPortName << endl;
        return false;
    }

    // ==================================================================
    // image port open
    imagePortLeft.open("/facetracking/image/left/in");  // give the left port a name

    // ==================================================================
    // robot
    // ==================================================================
    while(!Network::connect("/icub/camcalib/left/out", "/facetracking/image/left/in"))
    {
        Time::delay(3);
        cout << "try to connect left camera, please wait ..." << endl;
    }

    Property options;
    options.put("device", "remote_controlboard");
    options.put("local", "/tutorial/motor/client");
    options.put("remote", "/icub/head");

    robotHead = new PolyDriver(options);

    if(!robotHead->isValid())
    {
        cout << "Cannot connect to the robot head" << endl;
        return false;
    }

    robotHead->view(pos);
    robotHead->view(vel);
    robotHead->view(enc);
    robotHead->view(ictrl);

    if(pos==NULL || vel==NULL || enc==NULL || ictrl==NULL)
    {
        cout << "Cannot get interface to robot head" << endl;
        robotHead->close();
        return false;
    }
    jnts = 0;
    pos->getAxes(&jnts);

    setpoints.resize(jnts);
    cur_encoders.resize(jnts);
    prev_encoders.resize(jnts);

    /* prepare command */
    for(int i=0;i<jnts;i++)
    {
        setpoints[i] = 0;
    }

    for(int i=0; i<jnts; i++)
    {
        ictrl->setControlMode(i,VOCAB_CM_VELOCITY);
    }

    // ==================================================================
    //// create a opencv window
    cv::namedWindow("cvImage_Left",CV_WINDOW_NORMAL);

    // ==================================================================
    // face detection configuration
    face_classifier_left.load(xmlPath);

    attach(handlerPort);                  // attach to port

    // ==================================================================
    // Parameters
    counter = 0;
    x_buf = 0;
    y_buf = 0;

    mode = 0; // 0: going to a set position, 1: face searching, 2: face tracking, 3: face stuck,
    setpos_counter = 0;
    panning_counter = 0;
    stuck_counter = 0;
    tracking_counter = 0;

    // ==================================================================
    // random motion
    tilt_target = 0;
    pan_target = 0;

    seed = 10000;
    srand(seed);
    pan_max = 80;
    tilt_max = 20;

    cvIplImageLeft = NULL;



    return true ;
}

/************************************************************************/
bool faceTrackerModule::interruptModule() {
    cout << "Interrupting your module, for port cleanup" << endl;
    opc->interrupt();
    handlerPort.interrupt();
    return true;
}

/************************************************************************/
bool faceTrackerModule::close() {
    cout << "Calling close function" << endl;
    opc->close();
    handlerPort.close();

    cvReleaseImage(&cvIplImageLeft);
    robotHead->close();

    return true;
}

/************************************************************************/
bool faceTrackerModule::respond(const Bottle& command, Bottle& reply) {
    string helpMessage =  string(getName().c_str()) +
        " commands are: \n" +
        "track <string name> : track a face with the given opc name \n" +
        "track <int id> : track the object with the given opc id \n" +
        "track <double x> <double y> <double z> : track with the object coordinates\n" +
        "auto : switch attention between present objects \n" +
        "sleep : pauses the head control until next command" +
        "help \n" +
        "quit \n" ;

    reply.clear();

    if (command.get(0).asString()=="quit") {
        reply.addString("quitting");
        return false;
    }
    else if (command.get(0).asString()=="help") {
        cout << helpMessage;
        reply.addString("ok");
    }

    return true;
}

/************************************************************************/
double faceTrackerModule::getPeriod() {
    return 0.03;
}

/***************************************************************************/
bool faceTrackerModule::updateModule() {

    //unsigned long AAtime=0, BBtime=0; //check processing time
    //AAtime = cv::getTickCount(); //check processing time

    ImageOf<PixelRgb> *yarpImageLeft = imagePortLeft.read();
    //printf("Copying YARP image to an OpenCV/IPL image\n");

    if ( cvIplImageLeft == NULL )
    {
        cvIplImageLeft = cvCreateImage(cvSize(yarpImageLeft->width(),yarpImageLeft->height()),
                            IPL_DEPTH_8U,3);
    }
    cvCvtColor((IplImage*)yarpImageLeft->getIplImage(), cvIplImageLeft, CV_RGB2BGR);

    cv::Mat cvMatImageLeft=cv::cvarrToMat(cvIplImageLeft);

    //vector< cv::Mat > vImg;
    //cv::Mat rImg;
    //vImg.push_back(cvMatImageLeft);

    if(yarpImageLeft!=NULL) // check we actually got something
    {
        // resize images
        cv::resize(cvMatImageLeft, cvMatImageLeft, cv::Size(320, 240), 0,0,CV_INTER_NN);    //downsample 1/2x

        // convert captured frame to gray scale & equalize
        cv::Mat cvMatGrayImageLeft;
        cv::cvtColor(cvMatImageLeft, cvMatGrayImageLeft, CV_BGR2GRAY);
        cv::equalizeHist(cvMatGrayImageLeft, cvMatGrayImageLeft);

        // ==================================================================
        // face detection routine

        // a vector array to store the face found
        std::vector<cv::Rect> faces_left;;

        face_classifier_left.detectMultiScale(cvMatGrayImageLeft, faces_left,
            1.1, // increase search scale by 10% each pass
            3,   // merge groups of three detections
            CV_HAAR_DO_CANNY_PRUNING, //CV_HAAR_FIND_BIGGEST_OBJECT, //|CV_HAAR_SCALE_IMAGE, CV_HAAR_DO_CANNY_PRUNING
            cv::Size(30,30),
            cv::Size(50,50));

        int biggest_face_left_idx = 0;
        int biggest_face_left_size_buf = 0;

        // choosing the biggest face
        for(unsigned int i=0; i<faces_left.size(); i++)
        {
            cv::Point flb(faces_left[i].x + faces_left[i].width,
                          faces_left[i].y + faces_left[i].height);
            cv::Point ftr(faces_left[i].x, faces_left[i].y);

            cv::rectangle(cvMatImageLeft, flb, ftr, cv::Scalar(0,255,0), 3,4,0);

            if(biggest_face_left_size_buf < faces_left[i].height)
            {
                biggest_face_left_size_buf = faces_left[i].height;
                biggest_face_left_idx = i;
            }
        }

        //======================================================================================
        // Mode
        prev_encoders = cur_encoders;
        enc->getEncoders(cur_encoders.data());
//          cout << "Encoder: " << cur_encoders[0] << " " << cur_encoders[1] << " "<< cur_encoders[2] << '\n';

        ///////////////////////////
        // To set position mode
        if (mode == 0)
        {
            //-------------------------------------------------------------
            // Going to the set position mode
            if (setpos_counter < 100)
            {
                //cout << setpos_counter << endl;

                setpoints[0] = (0-cur_encoders[0])*0.3; // common tilt of head
                setpoints[1] = (0-cur_encoders[1])*0.3; // common roll of head
                setpoints[2] = (0-cur_encoders[2])*0.3; // common pan of head
                setpoints[3] = (0-cur_encoders[3])*0.3; // common tilt of eyes
                setpoints[4] = (0-cur_encoders[4])*0.3; // common pan of eyes

                setpos_counter++;
            }
            else
            {
                printf("Going to the set position is DONE!\n");

                setpoints[0] = 0;
                setpoints[2] = 0;
                setpoints[3] = 0;
                setpoints[4] = 0;

                mode = 1;
                printf("Face searching mode!\n");
                setpos_counter = 0;
            }
            vel->velocityMove(setpoints.data());
        }
        ///////////////////////////
        // Panning mode
        else if (mode == 1)
        {
            if(faces_left.size() > 0)
            {
                mode = 2;
                printf("I find a face!\n");
                panning_counter++;
            }
            else
            {
                //-------------------------------------------------------------
                // panning mode
                //printf("Face searching mode!\n");
                //cout << panning_target << endl;

                setpoints[0] = (tilt_target-cur_encoders[0])*0.3;   // common tilt of head
                setpoints[1] = (0-cur_encoders[1])*0.3; // common roll of head
                setpoints[2] = (pan_target-cur_encoders[2])*0.3;    // common pan of head
                setpoints[3] = (0-cur_encoders[3])*0.3; // common tilt of eyes
                setpoints[4] = (0-cur_encoders[4])*0.3; // common pan of eyes

                if ((abs(tilt_target - cur_encoders[0]) < 1) && (abs(pan_target - cur_encoders[2]) < 1))
                {
                    pan_r = ((double)rand() / ((double)(RAND_MAX)+(double)(1)));
                    tilt_r = ((double)rand() / ((double)(RAND_MAX)+(double)(1)));

                    pan_target = (int)((pan_r*pan_max)-(pan_max/2));
                    tilt_target = (int)((tilt_r*tilt_max)-(tilt_max/2));

                    //cout << pan_target << ", " << tilt_target << endl;
                }
            }

            vel->velocityMove(setpoints.data());
        }
        else if (mode == 2)
        {
            //-------------------------------------------------------------
            // face tracking mode
            if(faces_left.size() > 0)
            {
                double x = 320-(faces_left[biggest_face_left_idx].x + faces_left[biggest_face_left_idx].width/2);
                double y = 240-(faces_left[biggest_face_left_idx].y + faces_left[biggest_face_left_idx].height/2);

                //cout << "x:" << x << " y:" << y << '\n';

                x -= 320/2;
                y -= 240/2;

                double vx = x*0.3;  // Not to move too fast
                double vy = y*0.3;

                /* prepare command */
                for(int i=0;i<jnts;i++)
                {
                    setpoints[i] = 0;
                }

                setpoints[0] = vy;  // common tilt of head
                setpoints[2] = vx;  // common pan of head
                setpoints[3] = vy;  // common tilt of eyes
                setpoints[4] = -vx; // common pan of eyes

                x_buf = x;
                y_buf = y;
                counter = 0;

            // stopping smoothly
            }
            else if (faces_left.size() == 0 && counter < 10)
            {
                //cout << counter << endl;
                double vx = x_buf*0.3;  // Not to move too fast
                double vy = y_buf*0.3;

                /* prepare command */
                for(int i=0;i<jnts;i++)
                {
                    setpoints[i] = 0;
                }

                setpoints[0] = vy;  // common tilt of head
                setpoints[2] = vx;  // common pan of head
                setpoints[3] = vy;  // common tilt of eyes
                setpoints[4] = -vx; // common pan of eyes

                counter++;
            }
            else
            {
                printf("Hey! I don't see any face.\n");

                setpoints[0] = 0;
                setpoints[2] = 0;
                setpoints[3] = 0;
                setpoints[4] = 0;

                Time::delay(0.3);

                stuck_counter++;

                mode = 2;

                if(stuck_counter == 10)
                {
                    if(panning_counter > 5)
                    {
                        mode = 0;
                        printf("To a set position!\n");
                        stuck_counter = 0;
                    }
                    else
                    {
                        mode = 1;
                        printf("Face searching mode!\n");
                        stuck_counter = 0;
                    }
                }
            }
            vel->velocityMove(setpoints.data());

        }

        cv::imshow("cvImage_Left", cvMatImageLeft);

    }

    //BBtime = cv::getTickCount(); //check processing time

    cv::waitKey(1);

    return true;
}



