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
* icub-client/license/gpl.txt
*
* This program is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
* Public License for more details
*/

#ifndef _FACETRACKER_MODULE_H_
#define _FACETRACKER_MODULE_H_

#include <cstdio>
#include <iostream>
#include <ctime>
#include <cmath>

#include <opencv2/opencv.hpp>

#include <yarp/os/all.h>
#include <yarp/sig/all.h>
#include <yarp/dev/all.h>

/**
 * @ingroup faceTracker
 */
class faceTrackerModule : public yarp::os::RFModule {
    yarp::os::RpcServer handlerPort; //!< Response port
    yarp::os::BufferedPort<yarp::sig::ImageOf<yarp::sig::PixelRgb> > imagePortLeft; //!< A port for reading left images

    yarp::dev::IVelocityControl *vel;
    yarp::dev::IEncoders *enc;

    yarp::dev::PolyDriver *robotHead;

    yarp::sig::VectorOf<double> velocity_command;
    yarp::sig::VectorOf<double> cur_encoders;

    yarp::dev::IControlMode *ictrl;

    int getBiggestFaceIdx(const cv::Mat& cvMatImageLeft, const std::vector<cv::Rect> &faces_left);

protected:
    double x_buf;
    double y_buf;

    enum class Mode {GO_DEFAULT, FACE_SEARCHING, FACE_TRACKING, PAUSED}; //!< 0: going to default position, 1: face searching, 2: face tracking
    Mode mode;
    int counter_no_face_found; //!< if in face tracking mode, for how many iterations was no face found
    int setpos_counter; //!< how many iterations have we spend in default position mode
    int panning_counter; //!< how many times have we spend in face searching mode. If >5, go back to default position
    int stuck_counter;

    // random motion variables
    int tilt_target; //!< tilt target for face search mode (randomly chosen)
    int pan_target; //!< pan target for face search mode (randomly chosen)
    int pan_max; //!< maximum pan for the iCub
    int tilt_max; //!< maximum tilt for the iCub
    int nr_jnts; //!< number of joints of the iCub head

    cv::CascadeClassifier face_classifier_left;
    IplImage *cvIplImageLeft;

    /**
     * @brief Move to center position (after being stuck)
     */
    void moveToDefaultPosition();

    /**
     * @brief Randomly move around
     */
    void faceSearching(bool face_found);

    /**
     * @brief If a face was found, track it
     */
    void faceTracking(const std::vector<cv::Rect> &faces_left, int biggest_face_left_idx);

public:

    bool configure(yarp::os::ResourceFinder &rf); //!<  configure all the module parameters and return true if successful
    bool interruptModule();                       //!<  interrupt the ports
    bool close();                                 //!<  close and shut down the module
    bool respond(const yarp::os::Bottle& command, yarp::os::Bottle& reply);
    double getPeriod();
    bool updateModule(); //!< get an image from the robot's camera and decide which mode should be chosen next
};

#endif // __FACETRACKER_MODULE_H__
