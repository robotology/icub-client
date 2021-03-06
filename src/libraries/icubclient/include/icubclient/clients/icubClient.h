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

#ifndef __ICUBCLIENT_ICUBCLIENT_H__
#define __ICUBCLIENT_ICUBCLIENT_H__

#include <iostream>
#include <iomanip>
#include <fstream>
#include <tuple>
#include <typeindex>
#include <map>
#include <list>

#include <yarp/sig/Vector.h>

/**
* @defgroup icubclient_clients Clients
* \ingroup icubclient_libraries
*
* Provide a compact way to access the iCub functionalities within the icub-client framework.
*
*/

namespace icubclient{
    // forward declarations
    class OPCClient;

    class SubSystem;
    class SubSystem_agentDetector;
    class SubSystem_ARE;
    class SubSystem_babbling;
    class SubSystem_IOL2OPC;
    class SubSystem_KARMA;
    class SubSystem_Recog;
    class SubSystem_Speech;
    class SubSystem_SAM;
    class SubSystem_emotion;

    class Action;
    class Agent;
    class Entity;
    class Object;

    /**
    * \ingroup icubclient_clients
    *
    * Grants access to high level motor commands (grasp, touch, look, goto, etc) of the robot as well as its internal state
    * (drives, emotions, beliefs) and its interaction means (speech).
    */
    class ICubClient
    {
    protected:
        std::map<std::string, SubSystem*>  subSystems;
        bool                               closed;
        std::list<Action*>                 actionsKnown;

        //Reachability area
        double xRangeMin, yRangeMin, zRangeMin;
        double xRangeMax, yRangeMax, zRangeMax;

        std::string robot;  // Name of robot

    public:
        template <class T>
        T* getSubSystem(const std::string &subSystemName) {
            if (subSystems.find(subSystemName) == subSystems.end())
                return nullptr;
            else
                return (T*)subSystems[subSystemName];
        }

        /**
         * get a subsystem by just providing the type
         * untested, just to play around with C++11
         */
        template <class T>
        T* getSubSystem(T) {
            for(const auto subSystem : subSystems) {
                if(std::type_index(typeid(T)) == std::type_index(typeid(subSystem))) {
                    return subSystem;
                }
            }
            return nullptr;
        }

        SubSystem_agentDetector* getAgentDetectorClient();
        SubSystem_ARE* getARE();
        SubSystem_babbling* getBabblingClient();
        SubSystem_IOL2OPC* getIOL2OPCClient();
        SubSystem_KARMA* getKARMA();
        SubSystem_Recog* getRecogClient();
        SubSystem_Speech* getSpeechClient();
        SubSystem_SAM* getSAMClient();
        SubSystem_emotion* getEmotionClient();

        OPCClient*                  opc;
        Agent*                      icubAgent;

        /**
        * Create an iCub client
        * @param moduleName The port namespace that will precede the client ports names.
        * @param context The yarp context to be used.
        * @param clientConfigFile The yarp config file to be used.
        * @param isRFVerbose Whether the yarp resource finder should be verbose
        */
        ICubClient(const std::string &moduleName, const std::string &context = "icubClient",
            const std::string &clientConfigFile = "client.ini", bool isRFVerbose = false);

        /**
        * Try to connect all functionalities.
        * @param opcName the stem-name of the OPC server.
        * @return true in case of success false if some connections are missing.
        */
        bool connect(const std::string &opcName = "OPC");

        /**
        * Try to connect to OPC
        * @param opcName the stem-name of the OPC server.
        * @return true on success.
        */
        bool connectOPC(const std::string &opcName = "OPC");

        /**
        * Try to connect to all sub-systems.
        * @return true on success.
        */
        bool connectSubSystems();

        /**
        * Retrieve fresh definition of the iCub agent from the OPC
        */
        void updateAgent();

        /**
        * Commit the local definition of iCub agent to the OPC
        */
        void commitAgent();

        /**
        * Go in home position.
        * See the SubSystem_ARE::home documentation for more details.
        * @param part the part to be homed ("gaze", "head", "arms",
        *             "fingers", "all"; "all" by default).
        * @return true in case of successful motor command, false
        *         otherwise (e.g. wrong part name)
        */
        bool home(const std::string &part = "all");

        /**
        * Release the hand-held object on a given location.
        * See the SubSystem_ARE::release documentation for more details.
        * @param oLocation is the name of the entity in the OPC where the robot should release.
        * @param options bottle containing a list of options (e.g. force
        *                to use specific hand with "left"|"right"
        *                option).
        * @return true in case of success release, false otherwise
        *         (Entity non existing, impossible to reach, etc.).
        */
        bool release(const std::string &oLocation, const yarp::os::Bottle &options = yarp::os::Bottle());

        /**
        * Release the hand-held object on a given location.
        * See the SubSystem_ARE::release documentation for more details.
        * @param target contains spatial information about the location
        *               where releasing the object.
        * @param options bottle containing a list of options (e.g. force
        *                to use specific hand with "left"|"right"
        *                option).
        * @return true in case of success release, false otherwise
        *         (Entity non existing, impossible to reach, etc.).
        */
        bool release(const yarp::sig::VectorOf<double> &target, const yarp::os::Bottle &options = yarp::os::Bottle());

        /**
        * Point at a specified location from the iCub.
        * See the SubSystem_ARE::point documentation for more details.
        * @param target contains spatial information about the location
        *               where pointing at. The target can be far away from the iCub
        * @param options bottle containing a list of options (e.g. force
        *                to use specific hand with "left"|"right"
        *                option).
        * @return true in case of success release, false otherwise
        *         (Entity non existing, impossible to reach, etc.).
        */
        bool point(const yarp::sig::VectorOf<double> &target, const yarp::os::Bottle &options = yarp::os::Bottle());

        bool point(const double x, const double y, const double z, const yarp::os::Bottle &options = yarp::os::Bottle()) {
            yarp::sig::VectorOf<double> target(3);
            target[0] = x;
            target[1] = y;
            target[2] = z;
            return point(target, options);
        }

        /**
        * Point at a specified location.
        * See the SubSystem_ARE::point documentation for more details.
        * @param oLocation is the name of the entity in the OPC where the
        *              robot should point at.
        * @param options bottle containing a list of options (e.g. force
        *                to use specific hand with "left"|"right"
        *                option).
        * @return true in case of success release, false otherwise
        *         (Entity non existing, impossible to reach, etc.).
        */
        bool point(const std::string &oLocation, const yarp::os::Bottle &options = yarp::os::Bottle());

        /**
        * Enable/disable arms waving.
        * See the SubSystem_ARE::waving documentation for more details.
        * @param sw enable/disable if true/false.
        * @return true in case of successful request, false otherwise.
        */
        bool waving(const bool sw);

        /**
        * Push at a specified location.
        * This method is deprecated, and ICubClient::pushKarmaLeft or ICubClient::pushKarmaRight should be used instead.
        * See the SubSystem_ARE::push documentation for more details.
        * @param oLocation is the name of the entity in the OPC where the
        *              robot should push at.
        * @param options bottle containing a list of options (e.g. force
        *                to use specific hand with "left"|"right"
        *                option).
        * @return true in case of success release, false otherwise
        *         (Entity non existing, impossible to reach, etc.).
        */
        bool push(const std::string &oLocation, const yarp::os::Bottle &options = yarp::os::Bottle());

        /**
        * Take (grasp) an object.
        * See the subSystem_ARE::take documentation for more details.
        * @param oName is the name of the entity in the OPC where the
        *              robot should take at.
        * @param options bottle containing a list of options (e.g. force
        *                to use specific hand with "left"|"right"
        *                option).
        * @return true in case of success grasp, false otherwise
        *         (Entity non existing, impossible to reach, etc.).
        */
        bool take(const std::string &oName, const yarp::os::Bottle &options = yarp::os::Bottle());


        /**
         * @brief pushKarmaLeft: push an object by name to left side
         * See the SubSystem_KARMA::pushAside documentation for more details.
         * @param objName is the name of object, which will be looked for in OPC
         * @param targetPosYLeft: Y coordinate of object to push to
         * @param armType: "left" or "right" arm to conduct action, otherwise arm will be chosen by KARMA
         * @param options: options to be passed to Karma
         * @return true in case of success release, false otherwise
         */
        bool pushKarmaLeft(const std::string &objName, const double &targetPosYLeft,
            const std::string &armType = "selectable",
            const yarp::os::Bottle &options = yarp::os::Bottle());

        /**
         * @brief pushKarmaRight: push an object by name to right side
         * See the SubSystem_KARMA::pushAside documentation for more details.
         * @param objName is the name of object, which will be looked for in OPC
         * @param targetPosYRight: Y coordinate of object to push to
         * @param armType: "left" or "right" arm to conduct action, otherwise arm will be chosen by KARMA
         * @param options: options to be passed to Karma
         * @return true in case of success release, false otherwise
         */
        bool pushKarmaRight(const std::string &objName, const double &targetPosYRight,
            const std::string &armType = "selectable",
            const yarp::os::Bottle &options = yarp::os::Bottle());

        /**
         * @brief pushKarmaFront: push an object by name to front
         * See the SubSystem_KARMA::pushFront documentation for more details.
         * @param objName: name of object, which will be looked for in OPC
         * @param targetPosXFront: X coordinate of object to push to
         * @param armType: "left" or "right" arm to conduct action, otherwise arm will be chosen by KARMA
         * @param options: options to be passed to Karma
         * @return true in case of success release, false otherwise
         */
        bool pushKarmaFront(const std::string &objName, const double &targetPosXFront,
            const std::string &armType = "selectable",
            const yarp::os::Bottle &options = yarp::os::Bottle());

        /**
         * @brief pullKarmaBack: pull an object by name back
         * See the SubSystem_KARMA::pullBack documentation for more details.
         * @param objName: name of object, which will be looked for in OPC
         * @param targetPosXBack: X coordinate of object to pull back
         * @param armType: "left" or "right" arm to conduct action, otherwise arm will be chosen by KARMA
         * @param options: options to be passed to Karma
         * @return true in case of success release, false otherwise
         */
        bool pullKarmaBack(const std::string &objName, const double &targetPosXBack,
            const std::string &armType = "selectable",
            const yarp::os::Bottle &options = yarp::os::Bottle());

        /**
        * @brief pushKarma (KARMA): push to certain position, along a direction
        * See the SubSystem_KARMA::push documentation for more details.
        * @param targetCenter: position to push to.
        * @param theta: angle between the y-axis (in robot FoR) and starting position of push action, defines the direction of push action
        * @param radius: radius of the circle with center at @see targetCenter
        * @param options: options to be passed to Karma
        * @return true in case of success release, false otherwise
        */
        bool pushKarma(const yarp::sig::VectorOf<double> &targetCenter, const double &theta, const double &radius,
            const yarp::os::Bottle &options = yarp::os::Bottle());

        /**
        * @brief drawKarma (KARMA): draw action, along the positive direction of the x-axis (in robot FoR)
        * See the SubSystem_KARMA::draw documentation for more details.
        * @param targetCenter: center of a circle
        * @param theta: angle between the y-axis (in robot FoR) and starting position of draw action.
        * @param radius: radius of the circle with center at @see targetCenter
        * @param dist: moving distance of draw action
        * @param options: options to be passed to Karma
        * @return true in case of success release, false otherwise
        */
        bool drawKarma(const yarp::sig::VectorOf<double> &targetCenter, const double &theta,
            const double &radius, const double &dist,
            const yarp::os::Bottle &options = yarp::os::Bottle());

        /**
        * Look at a specified target object.
        * See the subSystem_ARE::look documentation for more details.
        * @param target is the name of the entity in the OPC where the robot should look.
        * @param options contains options to be passed on the gaze
        *                controller.
        */
        bool look(const std::string &target, const yarp::os::Bottle &options = yarp::os::Bottle());

        /**
        * Look at a specified target location.
        * See the subSystem_ARE::look documentation for more details.
        * @param target is a vector where the robot should look.
        * @param options contains options to be passed on the gaze
        *                controller.
        */
        bool look(const yarp::sig::VectorOf<double> &target, const yarp::os::Bottle &options = yarp::os::Bottle());

        /**
        * Looks at the agent if present in the scene.
        */
        bool lookAtPartner();

        /**
        * Looks at the agent specific bodypart if present in the scene.
        * @param sBodypartName is the name of the bodypart (kinect skeleton node) to be looked at
        */
        bool lookAtBodypart(const std::string &sBodypartName);

        /**
        * Extract the name of the agent interaction with the iCub (present, not iCub nor 'unnamed' partner)
        * @param verbose  - whether a yInfo output should happen if the partner is found
        * @return string: the name of the agent
        */
        std::string getPartnerName(bool verbose = true);

        /**
        * Extract the location of the bodypart name of the partner
        * @param sBodypartName is the name of the bodypart (kinect skeleton node) to be looked at
        * @return vLoc, the vector of the bodypart localisation
        */
        yarp::sig::VectorOf<double> getPartnerBodypartLoc(std::string sBodypartName);

        /**
        * %Babbling using a single joint.
        * See the SubSystem_babbling::babbling documentation for more details.
        * @param jointNumber contains the int corresponding to an arm joint
        * @param babblingArm contains the string corresponding to the side of the arm used ("left" or "right")
        * @param train_dur: set the babbling time, if "-1" use default time
        * @return true in case of success release, false otherwise
        */
        bool babbling(int jointNumber, const std::string &babblingArm, double train_dur = -1.0);

        /**
        * %Babbling with a single joint using the name of a corresponding bodypart
        * See the SubSystem_babbling::babbling documentation for more details.
        * @param bpName contains the string with the name of the bodypart
        * @param babblingArm contains the string corresponding to the side of the arm used ("left" or "right")
        * @param train_dur: set the babbling time, if "-1" use default time
        * @return true in case of success release, false otherwise
        *         (bodypart non existing, no joint number assigned, etc.).
        */
        bool babbling(const std::string &bpName, const std::string &babblingArm, double train_dur = -1.0);


        /**
        * Ask the robot to perform speech synthesis of a given sentence
        * @param text to be said.
        * @param shouldWait: Whether the method should wait until the speech was produced, or return immediately
        * @return true in case the sentence was uttered, false otherwise (speech subsystem not available)
        */
        bool say(const std::string &text, bool shouldWait = true);

        /**
        * Change the name of a given entity
        * @param e - the entity whose name should be changed
        * @param newName - the new name of the entity
        * @return true in case the name was changed, false otherwise
        */
        bool changeName(Entity *e, const std::string &newName);

        /**
        * Get the strongest emotion
        * @return tuple with name of emotion and intensity of emotion as items
        */
        std::tuple<std::string, double> getHighestEmotion();

        /**
        * Get the list of actions known the iCub
        * @return list of pointers to actions
        */
        std::list<Action*> getKnownActions();

        /**
        * Get the list of object that are in front of the iCub
        * Warning: this will update the local icubAgent
        * @return list of pointers to objects which are front of the iCub (x<0.0)
        */
        std::list<Object*> getObjectsInSight();

        /**
        * Get the list of objects that are graspable by the iCub
        * See isTargetInRange() for a definition of how an object is defined to be in range
        * Warning: this will update the local icubAgent
        */
        std::list<Object*> getObjectsInRange();

        /**
        * Check if a given cartesian position is within the reach of the robot
        * This can be adjusted using the reachingRangeMin and reachingRangeMax
        * parameters in the configuration file (client.ini)
        */
        bool isTargetInRange(const yarp::sig::VectorOf<double> &target) const;

        /**
        * Properly closes all ports which were opened. This is being called automatically
        * in the destructor, there is typically no need to call this manually.
        */
        void close();

        /**
        * Destructor.
        */
        virtual ~ICubClient();
    };
}//Namespace
#endif
