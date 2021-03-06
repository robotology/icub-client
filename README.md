iCub-HRI: A coherent framework for complex HRI scenarios on the iCub
=======

Generating complex, human-like behaviour in a humanoid robot like the iCub requires the integration of a wide range of open source components and a scalable cognitive architecture. Hence, we present the iCub-HRI library which provides convenience wrappers for components related to perception (object recognition, agent tracking, speech recognition, touch detection), object manipulation (basic and complex motor actions) and social interaction (speech synthesis, joint attention) exposed as C++ library with bindings for Python and Java (Matlab). In addition to previously integrated components, the library allows for simple extension to new components and rapid prototyping by adapting to changes in interfaces between components. We also provide a set of modules which make use of the library, such as a high-level knowledge acquisition module and an action recognition module. The proposed architecture has been successfully employed for a complex human-robot interaction scenario involving the acquisition of language capabilities, execution of goal-oriented behaviour and expression of a verbal narrative of the robot's experience in the world. Accompanying this paper is a tutorial which allows a subset of this interaction to be reproduced. The architecture is aimed at researchers familiarising themselves with the iCub ecosystem, as well as expert users, and we expect the library to be widely used in the iCub community. 

## Context

The code in this repository corresponds to a refactored and cleaned version of the [original repository](https://github.com/robotology/wysiwyd) produced in the context of the [WYSIWYD project](http://wysiwyd.upf.edu/) ((What You Say Is What You Did project). The research leading to these results has received funding from the European Research Council under the  European Union's Seventh Framework Programme (FP/2007-2013) / ERC Grant Agreement n. FP7-ICT-612139 (What You Say Is What You Did project).

## Documentation
Visit the official [project documentation](http://robotology.github.com/icub-hri).

## Citation
This code has an associated article published in the Frontiers in Robotics and AI. You may download the paper [here](https://www.frontiersin.org/articles/10.3389/frobt.2018.00022). The associated Zenodo repository is [![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.852665.svg)](https://doi.org/10.5281/zenodo.852665).

The citation is as follows ([bib file](https://www.tobiasfischer.info/publications/2018-Fischer-FRAI.bib)):
> T. Fischer, J.-Y. Puigbo, D. Camilleri, P. Nguyen, C. Moulin-Frier, S. Lallee, G. Metta, T. J. Prescott, Y. Demiris, and P. F. M. J. Verschure (2018). **iCub-HRI: A software framework for complex human robot interaction scenarios on the iCub humanoid robot. Frontiers in Robotics and AI** https://www.frontiersin.org/articles/10.3389/frobt.2018.00022

This code-oriented article complements a previous paper presenting the scientific concepts and results underlying this work ([bib file](https://www.tobiasfischer.info/publications/2017-MoulinFrier-TCDS.bib)):

> Moulin-Frier*, C., Fischer*, T., Petit, M., Pointeau, G., Puigbo, J., Pattacini, U., Low, S.C., Camilleri, D., Nguyen, P., Hoffmann, M., Chang, H.J., Zambelli, M., Mealier, A., Damianou, A., Metta, G., Prescott, T.J., Demiris, Y., Dominey, P.F.Verschure, P. F. M. J. (2017). **DAC-h3: A Proactive Robot Cognitive Architecture to Acquire and Express Knowledge About the World and the Self. IEEE Transactions on Cognitive and Developmental Systems.** http://doi.org/10.1109/TCDS.2017.2754143

(* *The two first authors contributed equally.*)

## Video demonstration
Please click on the image which refers to a YouTube video.
[![Video demonstration](http://img.youtube.com/vi/8A48EgPByAA/0.jpg)](http://www.youtube.com/watch?v=8A48EgPByAA "Video demonstration")

## License
The `icub-hri` library and documentation are distributed under the GPL-2.0.
The full text of the license agreement can be found in: [./LICENSE](https://github.com/robotology/icub-hri/blob/master/LICENSE).

Please read this license carefully before using the `icub-hri` code.

## CI Build
- Linux / Mac OS: [![Build Status](https://travis-ci.org/robotology/icub-hri.png?branch=master)](https://travis-ci.org/robotology/icub-hri)
- Windows: [![Build status](https://ci.appveyor.com/api/projects/status/mfxm27it64yycmff?svg=true)](https://ci.appveyor.com/project/robotology/icub-client)

## Build dependencies
`icub-hri` depends on the following projects which need to be installed prior to building `icub-hri`:

### YARP, icub-main and icub-contrib-common
First, follow the [installation instructions](http://wiki.icub.org/wiki/Linux:Installation_from_sources) for `yarp` and `icub-main`. If you want to use `iol`, `speech` or the `kinect-wrapper` (which are all optional), also install `icub-contrib-common`.

### OpenCV-3.2.0 (object tracking; optional)
**`OpenCV-3.0.0`** or higher (**`OpenCV-3.2.0`** is recommended) is a required dependency to build the `iol2opc` module which is responsible for object tracking. More specifically, we need the new tracking features delivered with `OpenCV-3.2.0`:

1. Download `OpenCV`: `git clone https://github.com/opencv/opencv.git`.
2. Checkout the correct branch: `git checkout 3.2.0`.
3. Download the external modules: `git clone https://github.com/opencv/opencv_contrib.git`.
4. Checkout the correct branch: `git checkout 3.2.0`.
5. Configure `OpenCV` by filling in the cmake var **`OPENCV_EXTRA_MODULES_PATH`** with the path pointing to `opencv_contrib/modules` and then toggling on the var **`BUILD_opencv_tracking`**.
6. Compile `OpenCV`.

### iol (object tracking; optional)
For the object tracking, we rely on the `iol` pipeline. Please follow the [installation instructions](https://github.com/robotology/iol). For `icub-hri`, not the full list of dependencies is needed. Only install the following dependencies: `segmentation`, `Hierarchical Image Representation`, and `stereo-vision`. Within `Hierarchical Image Representation`, we don't need `SiftGPU`.

To estimate the size and pose of objects, we rely on the `superquadric-model`. Please follow the [installation instructions](https://github.com/robotology/superquadric-model) if you want to use the `superquadric-model` (optional).

The compilation can be disabled using the `ICUBCLIENT_BUILD_IOL2OPC` cmake flag.

### kinect-wrapper (skeleton tracking; optional)
To detect the human skeleton, we employ the [`kinect-wrapper`](https://github.com/robotology/kinect-wrapper.git) library. Please follow the installation instructions in the [readme](https://github.com/robotology/kinect-wrapper/blob/master/README.md). It might be the case that you have also to build `kinect-wrapper` with the new `OpenCV-3.x.x` library. We have enabled the possibility to build only the client part of the `kinect-wrapper` (see [*updated instructions*](https://github.com/robotology/kinect-wrapper#cmaking-the-project)) which allows to run the `agentDetector` module on a separate machine as the one with the Kinect attached and running `kinectServer`.

In order to calibrate the Kinect reference frame with that of the iCub, we need to have (at least) three points known in both reference frames. To do that, we employ `iol2opc` to get the reference frame of an object in the iCub's root reference frame, and the `agentDetector` to manually find the corresponding position in the Kinect's reference frame. The `referenceFrameHandler` can then be used to find the transformation matrix between the two frames.

The procedure is as follows:
1. Start `iol2opc` (with its dependencies), `agentDetector --showImages` (after `kinectServer`) and `referenceFrameHandler` and connect all ports.
2. Place one object in front of the iCub (or, multiple objects with one of them being called "target"). Make sure this object is reliably detected by `iol2opc`.
3. Left click the target object in the depth image of the `agentDetector` window.
4. Move the object and repeat steps 3+4 at least three times.
5. Right click the depth image which issues a "cal" and a "save" command to `referenceFrameHandler`. This saves the transformation in a file which will be loaded the next time `referenceFrameHandler` is started.

### speech (speech recognition + synthesis; optional)
This requires a Windows machine with the [Microsoft speech SDK](https://msdn.microsoft.com/en-us/library/hh361572(v=office.14).aspx) installed. Then, compile the [`speech`](https://github.com/robotology/speech) repository for speech recognition and speech synthesis. If you use version 5.1 of the SDK, use this [patch](https://github.com/robotology/speech/tree/master/sapi5.1%20patch) to fix the compilation.

### karmaWYSIWYD (push/pull actions; optional)
Push and pull actions require the high-level motor primitives generator [karmaWYSIWYD](https://github.com/towardthesea/karmaWysiwyd), which facilitates users to control iCub push/pull actions on objects. Please follow the installation instructions in the [readme](https://github.com/towardthesea/karmaWYSIWYD/blob/master/README.md).

If you want to use `karmaWYSIWYD`, you must install `iol` / `iol2opc`. Then, start `iol2opc` and its dependencies as well as `iolReachingCalibration` and follow the instructions for [iolReachingCalibration](https://robotology.github.io/iol/doxygen/doc/html/group__iolReachingCalibration.html) to calibrate the arms before issuing any commands to `karmaWYSIWYD`.

### human-sensing-SAM (face recognition; optional)
The recognition of faces with SAM requires [human-sensing-SAM](https://github.com/dcam0050/human-sensing-SAM), to detect and output cropped faces for SAM to classify. Please follow the installation instructions in the [readme](https://github.com/dcam0050/human-sensing-SAM/blob/master/README.md).

## Build icub-hri

Once all desired dependencies are installed, building the `icub-hri` is straightforward:

1. Download `icub-hri`: `git clone https://github.com/robotology/icub-hri.git`.
2. `cd icub-hri`
3. `mkdir build`
4. `cd build`
5. `ccmake ..` and fill in the cmake var **`OpenCV_DIR`** with the path to the `OpenCV-3.2.0` build.
6. Compile `icub-hri` using `make`.

## Update software script
We provide a [Python script](https://github.com/robotology/icub-hri/blob/master/update-software.py) to easily update all dependencies of `icub-hri` and `icub-hri` itself.

## Using icub-hri in your project
Using `icub-hri` in your project is straightforward. Simply `find_package(icubclient REQUIRED)` in your main `CMakeLists.txt`, and use the `${icubclient_INCLUDE_DIRS}` and `${icubclient_LIBRARIES}` cmake variables as you would expect. A working example project can be found [here](https://github.com/robotology/icub-hri/tree/master/src/examples/icub-client-dependent-project).

## Docker image

The `docker` folder contains the dockerfile used to build a fully compiled dopcker image for icub-hri including all extras such as opencv3, iol, kinect and karmaWysiwyd. You can either download a pre-compiled image at https://hub.docker.com/r/dcamilleri13/icub-client or compile it using the provided files.

Prerequisites:
1. Install docker: https://docs.docker.com/engine/installation/linux/ubuntulinux/
2. Add permissions to user for convenience: http://askubuntu.com/questions/477551/how-can-i-use-docker-without-sudo
3. Install nvidia-docker. https://github.com/NVIDIA/nvidia-docker

To compile and run the image:
1. Run `make configure`. This will install any required libraries on the host system.
2. Run `make build`. This will compile the dockerfile into a docker image.
3. Run `make first_run`. This will run the dockerimage for the first time while setting up permissions, audio and video access. At this point the image is not ready to use because environment variables set by the dockerfile are not accessible via ssh. Thus at this point, type `exit` into the command line to close the image at which point a bashrc_iCub is created with all the required paths.
4. Run `make run` to launch and attach the docker image.

Note: This image was compiled with an Nvidia card present. It has not been tested on CPU only. Also please note that when running the docker image using docker run, ssh service is disabled on the host machine. This is due to the docker container mirroring the host's network configuration, the requirement for an ssh server to run inside of the container to communicate with pc104 and allow modules to be run via yarpmanager and the requirement for a single ssh server to be running on localhost.
