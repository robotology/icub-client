#!/usr/bin/env python
import matplotlib
matplotlib.use("TkAgg")
import matplotlib.pyplot as plt
import SAM
import sys
import subprocess
import os
from os import listdir
from os.path import isfile, join, isdir
import glob
import pkgutil
import time
import datetime
import signal
import pickle
import readline
import yarp
from ConfigParser import SafeConfigParser
import SAM.SAM_Core.SAM_utils as utils
import logging
import copy

## @ingroup icubclient_SAM_Core
class SamSupervisorModule(yarp.RFModule):
    """Model management and supervisor

        Description:
            Single point of entry for managing training and interaction with multiple models running in parallel. Parameters loaded from `default.ini` in `samSupervisor` context folder which contains the configuration details for samSupervisor as seen in the example below.

        Example:
            root_path	/usr/local/SAM_Data_Models
            config_path	sensory_level_conf.ini
            persistence	True
            windowed 	True
            verbose		True
            useOPC		False
            startModels     False
            acceptableDelay	"5"
            controllerIP    127.0.0.1

        Args:
            root_path   : Path to folder containing Data and Models folders.
            config_path	: Name of configuration file detailing parameters for different drivers.
            persistence	: `'True'` or `'False'` indicating terminals opened by samSupervisor stay open even after process termination.
            windowed 	: `'True'` or `'False'` indicating separate terminals will be opened by samSupervisor for each training and interaction process.
            verbose		: `'True'` or `'False'` switching on or off logging to stdout.
            useOPC		: `'True'` or `'False'` indicating using or not using OPC for further information.
            startModels : `'True'` or `'False'` indicating all models in the config_path file will be automatically loaded during samSupervisor startup.
            acceptableDelay	: String with an integer for the number of tries before a model is declared dead, aborted and restarted.
            controllerIP :  IP address for the ipyparallel controller. Currently not in use. Leave at 127.0.0.1.

    """
    def __init__(self):
        yarp.RFModule.__init__(self)
        self.SIGNALS_TO_NAMES_DICT = None
        self.terminal = 'xterm'
        self.rootPath = None
        self.interactionConfPath = None
        self.startModels = None
        self.persistence = None
        self.windowed = None
        self.verbose = None
        self.modelPath = None
        self.dataPath = None
        self.trainingFunctionsPath = None
        self.trainingListHandles = dict()
        self.loadedListHandles = dict()
        self.iter = 0
        self.rpcConnections = []
        self.inputBottle = yarp.Bottle()
        self.sendingBottle = yarp.Bottle()
        self.responseBottle = yarp.Bottle()
        self.outputBottle = yarp.Bottle()
        self.devnull = None
        self.supervisorPort = None
        self.interactionConfFile = None
        self.interactionParser = None
        self.interactionSectionList = None
        self.cluster = None
        self.functionsList = None
        self.trainableModels = None
        self.modelsList = None
        self.updateModels = None
        self.updateModelsNames = None
        self.noModels = None
        self.noModelsNames = None
        self.uptodateModels = None
        self.uptodateModelsNames = None
        self.nonResponsiveDict = dict()
        self.nonResponsiveThreshold = 5
        self.modelConnections = dict()
        self.connectionCheckCount = 0
        self.modelPriority = ['backup', 'exp', 'best']
        self.opcPort = None
        self.useOPC = None
        self.attentionModes = ['continue', 'stop']
        self.opcPortName = None
        self.opcRPCName = None
        self.baseLogFileName = 'samSupervisorErrorLog'

    def configure(self, rf):
        """
             Configure interactionSAMModel Yarp module.

            Args:
                rf: Yarp RF context input.

            Returns:
                Boolean indicating success or no success in initialising the Yarp module.
        """
        self.rootPath = rf.find("root_path").asString()
        file_i = 0
        loggerFName = join(self.rootPath, self.baseLogFileName + '_' + str(file_i) + '.log')

        # check if file exists
        while os.path.isfile(loggerFName) and os.path.getsize(loggerFName) > 0:
            loggerFName = join(self.rootPath, self.baseLogFileName + '_' + str(file_i) + '.log')
            file_i += 1

        logFormatter = logging.Formatter("%(asctime)s [%(name)-33s] [%(levelname)8s]  %(message)s")

        rootLogger = logging.getLogger('samSupervisor')
        rootLogger.setLevel(logging.DEBUG)

        fileHandler = logging.FileHandler(loggerFName)
        fileHandler.setFormatter(logFormatter)
        rootLogger.addHandler(fileHandler)

        consoleHandler = logging.StreamHandler()
        consoleHandler.setFormatter(logFormatter)
        rootLogger.addHandler(consoleHandler)
        logging.root = rootLogger

        logging.info(loggerFName)

        yarpAvailable = yarp.Network.checkNetwork()
        if not yarpAvailable:
            return False
        yarp.Network.init()
        self.SIGNALS_TO_NAMES_DICT = dict(
            (getattr(signal, n), n) for n in dir(signal) if n.startswith('SIG') and '_' not in n)

        # check if sam is already running by checking presence of /sam/rpc:i
        proc = subprocess.Popen(['yarp', 'ping', '/sam/rpc:i'], stdout=subprocess.PIPE)
        output = proc.stdout.read()
        proc.wait()
        del proc

        if output != '':
            logging.error('samSupervisor already running. /sam/rpc:i port present')
            return False

        rootPath = rf.check("root_path")
        interactionConfPath = rf.check("config_path")

        if not interactionConfPath and not rootPath:
            logging.error("Cannot find .ini settings")
            return False
        else:
            self.interactionConfPath = rf.find("config_path").asString()
            persistence = rf.check("persistence", yarp.Value("False")).asString()
            useOPC = rf.check("useOPC", yarp.Value("False")).asString()
            acceptableDelay = rf.check("acceptableDelay", yarp.Value("5")).asString()
            windowed = rf.check("windowed", yarp.Value("True")).asString()
            verbose = rf.check("verbose", yarp.Value("True")).asString()
            startModels = rf.check("startModels", yarp.Value("True")).asString()
            controllerIP = rf.check("controllerIP", yarp.Value("None")).asString()

            nodesGroupString = rf.findGroup('nodes').toString()
            logging.info('nodesGroupString length: ' + str(len(nodesGroupString)))
            if len(nodesGroupString) > 7:
                nodesList = nodesGroupString.replace('"', '').replace(')', '').split(' (')[1:]
                nodesDict = dict()
                for j in nodesList:
                    t = j.split(' ')
                    if utils.RepresentsInt(t[1]):
                        nodesDict[t[0]] = int(t[1])

                lenList = []
                for j in nodesDict.keys():
                    lenList.append(len(j))

                nodeMaxLen = max(lenList)
            else:
                nodesDict = dict()

            self.startModels = startModels == 'True'
            self.persistence = True if(persistence == "True") else False
            self.windowed = True if(windowed == "True") else False
            self.verbose = True if(verbose == "True") else False
            self.useOPC = True if (useOPC == "True") else False
            try:
                if int(acceptableDelay) > 5:
                    self.nonResponsiveThreshold = int(acceptableDelay)
                else:
                    logging.info('Requested responsive delay = ' + str(int(acceptableDelay)) + ' Minimum allowed = 5')

                    self.nonResponsiveThreshold = 5
            except:
                self.nonResponsiveThreshold = 5
            logging.info('Responsive Delay = ' + str(self.nonResponsiveThreshold))

            logging.info('Root supervisor path:     \t' + str(self.rootPath))
            logging.info('Model configuration file: \t' + str(self.interactionConfPath))
            logging.info('Bash Persistence set to:  \t' + str(self.persistence))
            logging.info('Windowed set to:          \t' + str(self.windowed))
            logging.info('Verbose set to:           \t' + str(self.verbose))
            logging.info('Controller IP:            \t' + str(controllerIP))
            logging.info('Nodes:')
            if len(nodesDict) > 0:
                for j in nodesDict.keys():
                    logging.info('                          \t', j.ljust(nodeMaxLen+4) +str(nodesDict[j]))
                logging.info('')

            self.modelPath = self.rootPath + '/Models'
            self.dataPath = self.rootPath + '/Data' 
            # OLD
            # self.trainingFunctionsPath = os.environ.get("ICUBCLIENT_DIR")+"/bin"
            # NEW
            self.trainingFunctionsPath = SAM.SAM_Drivers.__path__
            self.trainingListHandles = dict()
            self.loadedListHandles = dict()
            self.iter = 0
            self.rpcConnections = []
            self.inputBottle = yarp.Bottle()
            self.sendingBottle = yarp.Bottle()
            self.responseBottle = yarp.Bottle()
            self.outputBottle = yarp.Bottle()

            if not self.windowed:
                self.devnull = open('/dev/null', 'w')
            
            out = yarp.Bottle()
            self.checkAvailabilities(out)
            if self.verbose:
                logging.info(out.toString())

            self.supervisorPort = yarp.Port()
            self.supervisorPort.open('/sam/rpc:i')
            self.attach(self.supervisorPort)

            if self.useOPC:
                self.opcPort = yarp.RpcClient()
                self.opcPortName = '/sam/opcRpc:o'
                self.opcRPCName = '/OPC/rpc'
                self.opcPort.open(self.opcPortName)
                yarp.Network.connect(self.opcPortName, self.opcRPCName)

            # if len(nodesDict) > 0:
            #     self.cluster = utils.ipyClusterManager(nodesDict, controllerIP, self.devnull, totalControl=True)
            #     success = self.cluster.startCluster()
            #
            #     if not success:
            #         self.cluster = None
            #         cmd = 'ipcluster start -n 4'
            #         command = "bash -c \"" + cmd + "\""
            #
            #         if self.windowed:
            #             c = subprocess.Popen([self.terminal, '-e', command], shell=False)
            #         else:
            #             c = subprocess.Popen([cmd], shell=True)
            #
            #         self.trainingListHandles['Cluster'] = c

            if len(self.uptodateModels) + len(self.updateModels) > 0:
                if self.verbose:
                    logging.info("Loading models according to " + self.interactionConfPath)
                # start loading model configuration according to interactionConfPath file
                
                rfModel = yarp.ResourceFinder()
                rfModel.setVerbose(self.verbose)
                rfModel.setDefaultContext("samSupervisor")
                self.interactionConfFile = rfModel.findFile(self.interactionConfPath)
                
                # Iterate over all sections within the interactionConfPath,
                # create a list and check against the available models
                # warn if model specified in interactionConfPath not loadable
                self.interactionParser = SafeConfigParser()
                self.interactionParser.read(self.interactionConfFile)
                self.interactionSectionList = self.interactionParser.sections()
                if self.verbose:
                    logging.info('')
                    logging.info(self.dataPath)
                    logging.info(self.interactionSectionList)
                    logging.info('')
                if self.startModels:
                    for j in self.interactionSectionList:
                        command = yarp.Bottle()
                        command.addString("load")
                        command.addString(j)
                        if self.verbose:
                            logging.info(command.toString())
                        reply = yarp.Bottle()

                        self.loadModel(reply, command)
                        if self.verbose:
                            logging.info(reply.toString())
                            logging.info("-----------------------------------------------")
                            logging.info('')
                else:
                    logging.info('Config ready. Awaiting input ...')
            elif len(self.noModels) > 0:
                if self.verbose:
                    logging.info("Models available for training.")
                # Train a model according to ineractionConfPath file
            else:
                if self.verbose:
                    logging.info("No available models to load or train")
                # wait for a training command
                
            return True

    def close(self):
        """
            Close Yarp module.

            Description:
                Starts by issuing an `EXIT` command to all loaded interaction modules. Terminates training processes. Closes samSupervisor ports and terminates cluster if one has been started.

            Args:
                None

            Returns:
                Boolean indicating success or no success in closing the Yarp module.
        """
        # close ports of loaded models
        for j in self.rpcConnections:
            j[1].write(yarp.Bottle('EXIT'), self.inputBottle)
            j[1].interrupt()
            time.sleep(1)
            j[1].close()

        self.supervisorPort.interrupt()
        self.supervisorPort.close()

        if self.opcPort is not None:
            self.opcPort.interrupt()
            self.opcPort.close()

        for i, v in self.trainingListHandles.iteritems():
            v.send_signal(signal.SIGINT)
            v.wait()   

        for v in self.rpcConnections:
            v[4].send_signal(signal.SIGINT)
            v[4].wait()

        if self.cluster is not None:
            self.cluster.terminateProcesses()

    def checkAvailabilities(self, reply):
        """
            Check model availabilities

            Description:
                This function starts by compiling a list of models present in the `root_path/Models`. Then cross checks that the corresponding data folder exists in `root_path/Data` and that it contains an appropriate `config.ini` file. If a data folder exists but a model does not, this is added to __noModels__ list. If the folder and the model both exist and the modification date of the data is newer than that of the model it is added to __updateModels__ (requiring an update) if the modification date of the model is newer it is added to __uptodateModels__. If the model exists but not the data folder config file, the model is ignored. Each model and data folder pair is then paired with its respective driver from the SAM_Drivers folder. If a corresponding driver is not available, the data and model are ignored.

            Args:
                reply : Yarp Bottle containing a many string formatted response to indicate the state of all the available drivers and the corresponding models.

            Returns:
                None
        """
        # after finding the root path, go to models folder and compile list of all
        # models together with the last time they were modified
        onlyfiles = [f for f in listdir(self.modelPath) if isfile(join(self.modelPath, f))]

        # find number of .pickle files
        self.modelsList = [s.replace(".pickle", "") for s in onlyfiles
                           if ".pickle" in s and '~' not in s and '__L' not in s]
        if self.verbose:
            logging.info('Models available:                ' + ', '.join(self.modelsList))

        # likewise go to data folder and compile list of all folders and last time they were modified
        dataList = [f for f in listdir(self.dataPath) if isdir(join(self.dataPath, f))]
        if self.verbose:
            logging.info("Data folders available:          " + ', '.join(dataList))

        # likewise parse training functions folder
        # OLD
        # self.functionsList = [f.replace(".py","") for f in listdir(self.trainingFunctionsPath)
        #                       if isfile(join(self.trainingFunctionsPath, f)) if ".py" in f if '~' not in f]
        # self.functionsList.sort()
        # NEW
        self.functionsList = []
        for importer, modname, ispkg in pkgutil.iter_modules(SAM.SAM_Drivers.__path__): 
            if 'SAMDriver_' in modname:
                self.functionsList += [modname]
        self.functionsList.sort()

        if self.verbose:
            logging.info("Training functions available:    " + ', '.join(self.functionsList))

        # format of training functions is expected to be train_modelName_anythingElseToDistinguish
        # therefore data folders must contain .ini file pointing towards the preferred algorithm to be chosen
        model_params = ["model_options"]
        if self.verbose:
            logging.info('-------------------')
            logging.info('Finding trainable data ...')
            logging.info('')
        # exit if no training functions have been found
        if len(self.functionsList) == 0:
            if self.verbose:
                logging.error("No training functions found. Exiting ...")
            return False
        else:
            self.trainableModels = []
            # check which data folders are trainable i.e training functions available
            for f in dataList:
                loc = join(self.dataPath, f)
                if self.verbose:
                    logging.info("Checking " + loc + " ...")
                try:
                    parser = SafeConfigParser()
                    found = parser.read(loc + "/config.ini")
                    if not found:
                        if self.verbose:
                            logging.warning("config.ini not found for " + str(f))
                        pass
                    else:
                        if parser.has_section(model_params[0]):
                            try:
                                # NEW
                                trainOptions = parser.get(model_params[0], 'driver').split(',')
                                # OLD
                                # trainOptions = parser.get(model_params[0], 'train').split(',')
                                # check training function exists
                                availableFuncs = [s for s in trainOptions for g in self.functionsList if s == g]
                                if len(availableFuncs) != 0:
                                    if self.verbose:
                                        logging.info("Training functions for data " + f + " are " + ','.join(trainOptions))
                                        logging.info("Corresponding functions available: " + ','.join(availableFuncs))
                                    
                                    if len(availableFuncs) > 1:
                                        if self.verbose:
                                            logging.info("The first function will be chosen: " + availableFuncs[0])
                                    # find latest modified date of directory and subdirectories
                                    # thus checking for addition of new data
                                    t = []
                                    for dirName, dirs, filenames in os.walk(loc):
                                        t.append(os.path.getmtime(dirName))
                                    lastMod = max(t)
                                    if self.verbose:
                                        logging.info("Data folder last modified: %s" % time.ctime(lastMod))
                                    # format of trainableModels is: dataFolder name, corresponding training function,
                                    # date data last modified, train boolean
                                    self.trainableModels += [[f, availableFuncs[0], lastMod, True]]
                                else:
                                    if self.verbose:
                                        logging.warning("Training functions for data " + f + \
                                                     " not found. Will not train " + f)
                            except:
                                logging.warning("No option 'driver' in section: 'model_options' for " + f)
                        else:
                            if self.verbose:
                                logging.warning("Training parameters for data " + f + " not found. Will not train " \
                                      + f + "\nCheck config.ini is formatted correctly")
                except IOError:
                    pass
                if self.verbose:
                    logging.info('')
            if self.verbose:
                logging.info('-------------------')
                logging.info('Checking corresponding models')
                logging.info('')
            # compare models and data folders. Assuming model names = folder names
            # check if model exists
            for f in self.trainableModels:
                t = []
                currModels = []
                for g in self.modelsList:
                    if str(f[0]) + '_' in g and '~' not in g:
                        # compare time of model and data
                        currModels.append(g)
                        g += ".pickle"
                        loc = join(self.modelPath, g)
                        t.append(os.path.getmtime(loc))
                if len(t) > 0:
                    lastMod = max(t)

                    currModelsDict = dict()
                    currModelsDict['exp'] = ''
                    currModelsDict['best'] = ''
                    currModelsDict['backup'] = ''
                    for l in t:
                        tempModel = currModels[t.index(l)]
                        if 'exp' in tempModel:
                            currModelsDict['exp'] = tempModel
                        elif 'best' in tempModel:
                            currModelsDict['best'] = tempModel
                        elif 'backup' in tempModel:
                            currModelsDict['backup'] = tempModel

                    # currModels = currModels[t.index(lastMod)]
                    # self.trainableModels[self.trainableModels.index(f)].append(currModels)
                    self.trainableModels[self.trainableModels.index(f)].append(currModelsDict)

                    if self.verbose:
                        logging.info(str(f[0]) + " Model last modified: %s" % time.ctime(lastMod))
                    if lastMod < f[2]:
                        tdiff = datetime.datetime.fromtimestamp(f[2]).replace(microsecond=0) - \
                                datetime.datetime.fromtimestamp(lastMod).replace(microsecond=0)
                        if self.verbose:
                            logging.info(str(f[0]) + ' Model outdated by ' + str(tdiff) + '. Will be trained')
                    else:
                        if self.verbose:
                            logging.info(str(f[0]) + ' Model up-to-date')
                        f[3] = False
                else:
                    self.trainableModels[self.trainableModels.index(f)].append('')
                    if self.verbose:
                        logging.info(str(f[0]) + ' Model not found. Training Required')
                if self.verbose:
                    logging.info('')
            if self.verbose:
                logging.info('-------------------')
                logging.info('')

            # provide option to train now or on close
            # if train now provide option to change experiment number or leave default
            self.updateModels = [s for s in self.trainableModels if s[3] if s[4] != '']
            self.updateModelsNames = [s[0] for s in self.trainableModels if s[3] if s[4] != '']

            self.noModels = [s for s in self.trainableModels if s[3] if s[4] == '']
            self.noModelsNames = [s[0] for s in self.trainableModels if s[3] if s[4] == '']

            self.uptodateModels = [s for s in self.trainableModels if not s[3]]
            self.uptodateModelsNames = [s[0] for s in self.trainableModels if not s[3]]
            
            reply.addVocab(yarp.Vocab_encode("many"))
            reply.addString(str(len(self.uptodateModels)) + " Models up-to-date " + str(self.uptodateModelsNames))
            reply.addString(str(len(self.updateModels)) + " Models require an update " + str(self.updateModelsNames))
            reply.addString(str(len(self.noModels)) + " new models to train " + str(self.noModelsNames))
            reply.addString('')

            for j in self.updateModelsNames + self.uptodateModelsNames + self.noModelsNames:
                rep = yarp.Bottle()
                cmd = yarp.Bottle()
                cmd.addString('check')
                cmd.addString(j)
                self.checkModel(rep, cmd, allCheck=True)
                a = str(rep.toString())
                reply.addString(a)

            return True

    def respond(self, command, reply):
        """
            Respond to external requests

            Description:
                Available requests \n
                1) __askOPC__               :   Query the OPC for additional contextual data. calls askOPC() \n
                2) __attentionModulation <>__  :   Modulate attention of all models. Enable classification if <param> is `continue`. Disable classification if <param> is `stop`. Calls attentionModulation() \n
                3) __check_all__            :   Cross checks available models, data folders, config files and driver availability. Calls checkAvailabilities() \n
                4) __check <modelName>__    :   Cross check available models, data folders, config files and driver availability for a specific <modelName>. Calls checkModel()\n
                5) __config <modelName>__   :   Opens the data folder config file for the specific <modelName> to view or change the training parameters. Calls configModel() \n
                6) __dataDir <modelName>__  :   Returns the model filename that will be used for the specific <modelName>. Calls dataDirModel() \n
                7) __delete <modelName>__   :   Deletes model from hard disk. Calls deleteModel() \n
                8) __help__                 :   Returns a many formatted list of available commands \n
                9) __load <modelName>__     :   Launches interactionSAMModel.py for the <modelName>. Calls loadModel() \n
                10) __close <modelName>__    :   Terminates the interactionSAMModel.py process for the <modelName>. Calls closeModel() \n
                11) __optimise <modelName>__ :   Launches samOptimiser.py to optimise the training of the specified model. Calls optimise().\n
                12) __quit__                 :   Closes all loaded models and stops samSupervisor.\n
                13) __report <modelName> <plotFlag>__   :   Reports the confusion matrix of <modelName> if it is a trained model. Plot result if <plotFlag> is set to `plot`. Return a formatted confusion matrix if <plotFlag> is not `plot`. Calls reportModel()\n
                14) __train <modelName>__    :   Launches trainSAMModel.py to train the specified model. Calls train() \n
                15) __list_callSigns__       :   Returns a list of the `ask_X_label` and `ask_X_instance` call signs for all models that are currently loaded with interactionSAMModel.py\n
                7) __ask_X_label__    :  Forwards the received ask label request to the corresponding loaded model if the callsign is present in __list_callsigns__. This command has an enforced timeout of 10 seconds to respond otherwise returns `nack`. \n
                8) __ask_X_instance__ :  Forwards the received ask instance request to the corresponding loaded model if the callsign is present in __list_callsigns__. This command has an enforced timeout of 10 seconds to respond otherwise returns `nack`. \n

            Args:
                command : Incoming Yarp bottle containing external request.
                reply : Outgoing Yarp bottle containing reply to processed request.

            Returns:
                Boolean indicating success or no success in responding to external requests.
        """
        helpMessage = ["Commands are: ", "\taskOPC", "\tattentionModulation mode", "\tcheck_all", "\tcheck modelName",
                       "\tclose modelName", "\tconfig modelName", "\tdataDir modelName", "\tdelete modelName", "\thelp",
                       "\tlistModels mode", "\tload modelName", "\toptimise modelName", "\tquit", "\treport modelName", "\ttrain modelName",
                       "\tlist_callSigns"]
        b = yarp.Bottle()
        self.checkAvailabilities(b)
        reply.clear()

        if command.get(0).asString() == "askOPC":
            self.askOPC(reply)
        elif command.get(0).asString() == "attentionModulation":
            self.attentionModulation(reply, command)
        elif command.get(0).asString() == "check_all":
            self.checkAvailabilities(reply)
        elif command.get(0).asString() == "check":
            self.checkModel(reply, command)
        elif command.get(0).asString() == "listModels":
            self.listModels(reply, command)
        elif command.get(0).asString() == "close":
            self.closeModel(reply, command, True)
        elif command.get(0).asString() == "delete":
            self.deleteModel(reply, command)
        elif command.get(0).asString() == "report":
            self.reportModel(reply, command)
        elif command.get(0).asString() == "dataDir":
            self.dataDirModel(reply, command)
        elif command.get(0).asString() == "config":
            self.configModel(reply, command)
        elif command.get(0).asString() == "help":
            reply.addVocab(yarp.Vocab_encode("many"))
            for i in helpMessage:
                reply.addString(i)
        elif command.get(0).asString() == "load":
            self.loadModel(reply, command)
        elif command.get(0).asString() == "quit":
            reply.addString("quitting")
            return False 
        elif command.get(0).asString() == "train":
            self.train(reply, command)
        elif command.get(0).asString() == "optimise":
            self.optimise(reply, command)
        elif command.get(0).asString() == "list_callSigns":
            reply.addVocab(yarp.Vocab_encode("many"))
            for e in self.rpcConnections:
                repStr = str(e[0]) + " Model: \t"
                for f in e[3]:
                    repStr += str(f) + "\t"
                reply.addString(repStr)

        elif any(command.get(0).asString() in e[3] for e in self.rpcConnections):
            if 'instance' in command.get(0).asString() and command.size() != 2:
                reply.addString("Instance name required. e.g. ask_face_instance Daniel")
            else:
                try:
                    self.forwardCommand(command, reply)
                    if reply.size() == 0:
                        reply.addString('nack')
                        reply.addString('No response')
                except utils.TimeoutError:
                    reply.addString('nack')
                    reply.addString('Failed to respond within timeout')

        else:
            reply.addString("nack")
            reply.addString("Wrong command")
            # reply.addVocab(yarp.Vocab_encode("many"))
            # reply.addString("Wrong command. ")
            # for i in helpMessage:
            #     reply.addString(i)
            # reply.addString("Call signs available:")
            # for e in self.rpcConnections:
            #     repStr = "\t" + e[0] + " Model: \t"
            #     for f in e[3]:
            #         repStr += str(f) + "\t"
            #     reply.addString(repStr)
        return True

    @utils.timeout(10)
    def forwardCommand(self, command, reply):
        """
            Helper function to forward a call sign  to the respective model with an enforced timeout of 10 seconds for the reply so the module does not hang.

            Args:
                command : Yarp bottle with the call sign.
                reply   : Yarp bottle for the reply from the model.

            Returns:
                None
        """
        for e in self.rpcConnections:
            if command.get(0).asString() in e[3]:
                e[1].write(command, reply)

    def interruptModule(self):
        """
            Module interrupt logic.

            Returns : Boolean indicating success of logic or not.
        """
        return True

    def listModels(self, reply, command):
        """ Returns lists of models for train or interaction categpories.

            Args:
                command : Yarp bottle with command. Example valid commands below.
                reply   : Yarp bottle for the reply from the model.

            Returns :
                Boolean indicating success or not.

            Example :
                listModels train - returns list of models that require training\n
                listModels interaction - returns list of interaction ready models\n
                listModels all - returns a list of all models

        """
        logging.info(command.toString())
        b = yarp.Bottle()
        self.checkAvailabilities(b)

        reply.clear()
        if command.size() < 2:
            reply.addString("nack")
            reply.addString("'train', 'interaction' or `all` required. eg listModels train")
        elif command.get(1).asString() not in ["train", "interaction", "all"]:
            reply.addString("nack")
            reply.addString("'train', 'interaction' or `all` required. eg listModels train")
        else:
            if command.get(1).asString() == "train":
                listOfModels =  self.updateModelsNames + self.noModelsNames
            elif command.get(1).asString() == "interaction":
                listOfModels = self.updateModelsNames + self.uptodateModelsNames
            elif command.get(1).asString() == "all":
                listOfModels =  self.updateModelsNames + self.noModelsNames + self.uptodateModelsNames

            reply.addString("ack")
            for j in listOfModels:
                reply.addString(j)

        return True

    def attentionModulation(self, reply, command):
        """ Modulate attention of all models.

            Args:
                command : Yarp bottle with command. Example valid commands below.
                reply   : Yarp bottle for the reply from the model.

            Returns :
                Boolean indicating success or not.

            Example :
                attentionModulation stop \n
                attentionModulation continue

        """
        reply.clear()
        logging.info(command.toString())
        if command.size() < 2:
            reply.addString("nack")
            reply.addString("'stop' or 'continue' required. eg attentionModulation stop")
        elif command.get(1).asString() not in self.attentionModes:
            reply.addString("nack")
            reply.addString("'stop' or 'continue' required. eg attentionModulation stop")
        else:
            for j in self.rpcConnections:
                rep = yarp.Bottle()
                cmd = yarp.Bottle()
                cmd.addString('attention')
                cmd.addString(command.get(1).asString())
                j[1].write(cmd, rep)
            reply.addString('ack')
        return True

    def askOPC(self, reply):
        """ Query the OPC for additional contextual data.

            Description:
                Query name of agent from OPC and forward it to Actions model if it is loaded. This is currently the only model that uses this method. Method will be generalised in future versions.

            Args:
                reply   : Yarp bottle for the reply from OPC.

            Returns :
                Boolean indicating success or not.
        """
        reply.clear()
        actionsLoadedList = [t for t in self.rpcConnections if 'Actions' in t[0]]

        # check network connection with OPC is present if not make it
        if self.opcPort.getOutputCount() == 0:
            yarp.Network.connect(self.opcPortName, self.opcRPCName)

        if self.opcPort.getOutputCount() > 0:
            if len(actionsLoadedList) > 0:
                # ask for all objects with item entity
                cmd = yarp.Bottle()
                cmd.fromString('[ask] (("entity" "==" "agent"))')

                rep = yarp.Bottle()
                self.opcPort.write(cmd, rep)
                # split items in the returned string
                lID = rep.get(1).toString().split('(')[-1].replace(')', '').split(' ')

                # iterate over items to get agent name
                agentList = []
                for j in lID:
                    cmd = yarp.Bottle()
                    cmd.fromString('[get] (("id" ' + str(j) + ') (propSet ("name")))')
                    rep = yarp.Bottle()
                    self.opcPort.write(cmd, rep)
                    agentList.append(rep.toString().split('name')[-1].split(')')[0].replace(' ', ''))

                currAgent = [t for t in agentList if t != 'icub']

                if len(currAgent) != 0:
                    currAgent = currAgent[0]
                    if len(currAgent) > 0:
                        for j in actionsLoadedList:
                            cmd = yarp.Bottle()
                            cmd.addString('information')
                            cmd.addString('partnerName')
                            cmd.addString(currAgent)
                            rep = yarp.Bottle()
                            j[1].write(cmd, rep)

                        reply.addString('ack')
                        reply.addString('Agent = ' + str(currAgent))
                    else:
                        reply.addString('nack')
                        reply.addString('No agent apart from icub present')
                else:
                    reply.addString('nack')
                    reply.addString('No agent apart from icub present')
            else:
                reply.addString('ack')
                reply.addString('No actions loaded')
        else:
            reply.addString('nack')
            reply.addString('OPC port not present')
            logging.warning('OPC not found!')

        return True

    def closeModel(self, reply, command, external=False):
        """ Close a loaded model.

            Description:
                Check that a model has been loaded and if it is close the loaded model.

            Args:
                reply   : Yarp bottle for the reply from the function.
                command : Yarp bottle with the command and the model name to close.
                external: Boolean indicating model to be closed is to be closed for good if `True`. Else model is to be closed for restart.

            Returns :
                Boolean indicating success or not.
        """
        if command.size() != 2:
            reply.addString('nack')
            reply.addString("Model name required. e.g. close Actions")
        else:
            alreadyOpen = False
            conn = -1
            for k in range(len(self.rpcConnections)):
                if self.rpcConnections[k][0] == command.get(1).asString():
                    alreadyOpen = True
                    conn = k

            logging.info("Already open = " + str(alreadyOpen))
            if self.verbose:
                logging.info(command.get(1).asString())
            if alreadyOpen:
                if external:
                    if command.get(1).asString() in self.modelConnections.keys():
                        self.modelConnections[command.get(1).asString()] = dict()
                self.rpcConnections[conn][-1] = 'loading' # this will make it ignored by the non responsive countdown
                self.rpcConnections[conn][1].write(yarp.Bottle('EXIT'), self.inputBottle)
                self.rpcConnections[conn][1].interrupt()
                time.sleep(1)
                self.rpcConnections[conn][1].close()
                time.sleep(1)
                self.rpcConnections[conn][4].send_signal(signal.SIGINT)
                self.rpcConnections[conn][4].wait()
                del self.rpcConnections[conn]
                reply.addString('ack')
                reply.addString(command.get(1).asString() + " model closed.")
            else:
                reply.addString('ack')
                reply.addString(command.get(1).asString() + " model is not running.")
        return True

    def loadModel(self, reply, command):
        """ Load a trained model

            Description:
                 Launches interactionSAMModel.py for the model mentioned in the command.

            Args:
                reply   : Yarp bottle for the reply from the function.
                command : Yarp bottle with the command and the model name to load.

            Returns :
                None
        """
        parser = SafeConfigParser()

        if command.size() < 2:
            reply.addString('nack')
            reply.addString("Model name required. e.g. load Actions")
        elif command.get(1).asString() in self.noModelsNames:
            reply.addString('nack')
            reply.addString("Cannot load model. Model training available but not yet trained.")
        elif command.get(1).asString() in self.uptodateModelsNames+self.updateModelsNames:
            ret = parser.read(join(self.dataPath, command.get(1).asString(), "config.ini"))
            if len(ret) > 0:
                # OLD
                # if(parser.has_option('model_options', 'interaction')):
                #    interactionFunction = parser.get('model_options', 'interaction').split(',')
                # NEW
                if parser.has_option('model_options', 'driver') and parser.has_option('model_options', 'modelNameBase'):
                    interactionFunction = parser.get('model_options', 'driver').split(',')
                    # modelNameBase = parser.get('model_options', 'modelNameBase')
                    
                    interactionFunction = [s for s in interactionFunction for g in self.functionsList if s == g]
                    if len(interactionFunction) != 0:
                        j = [s for s in self.trainableModels if s[0] == command.get(1).asString()][0]

                        interfacePortName = self.interactionParser.get(j[0], 'rpcBase') + ':o'
                        callSignList = self.interactionParser.get(j[0], 'callSign').replace(' ', '').split(',')
                        
                        # check if the model is already loaded
                        alreadyOpen = False
                        correctOperation = False
                        conn = -1
                        for k in range(len(self.rpcConnections)):
                            if self.rpcConnections[k][0] == j[0]:
                                alreadyOpen = True
                                conn = k

                        logging.info("Loading " + str(interfacePortName) + " with " + str(callSignList))
                        if alreadyOpen:
                            if self.verbose:
                                logging.info("Model already open")
                            # check it is functioning correctly
                            correctOp_check1,  correctOp_check2 = self.checkOperation(self.rpcConnections[conn])
                            correctOperation = correctOp_check1 and correctOp_check2
                            if not correctOp_check2 and correctOp_check1:
                                alreadyOpen = False

                            if self.verbose:
                                logging.info("correct operation = " + str(correctOperation))
                                logging.info('')
                        else:
                            if self.verbose:
                                logging.info("Model not open")
                        
                        if alreadyOpen and correctOperation:
                            rep = yarp.Bottle()
                            cmd = yarp.Bottle()
                            cmd.addString("reload")
                            # logging.info(self.rpcConnections[conn][0], 'reload')
                            self.rpcConnections[conn][1].write(cmd, rep)
                            if rep.get(0).asString() == 'ack':
                                reply.addString('ack')
                                reply.addString(command.get(1).asString() + " model re-loaded correctly")
                            else:
                                reply.addString('nack')
                                reply.addString(command.get(1).asString() + " model did not re-loaded correctly")
                        elif alreadyOpen and not correctOperation:
                            # terminate model by finding process in self.rpcConnections[4]
                            alreadyOpen = False
                            rep = yarp.Bottle()
                            cmd = yarp.Bottle()
                            cmd.addString("close")
                            cmd.addString(command.get(1).asString())
                            self.closeModel(rep, cmd)
                            reply.addString('ack')
                            reply.addString(command.get(1).asString() + " model terminated ")

                        if not alreadyOpen:
                            interfacePort = yarp.RpcClient()
                            successfulOpen = interfacePort.open(interfacePortName)

                            if not successfulOpen:
                                logging.error('CRAP')
                                for jk in self.rpcConnections:
                                    logging.info(jk[0])
                            
                            # OLD
                            # args = ' '.join([join(self.dataPath,j[0]), join(self.modelPath, j[4]),
                            #                  self.interactionConfFile])
                            # cmd = 'ipython ' + join(self.trainingFunctionsPath, interactionFunction[0]+'.py') + \
                            #       ' -- ' + args
                            # NEW
                            modType = command.get(2).asString()
                            if modType != '' and modType in j[4].keys():
                                if j[4][modType] != '':
                                    modToLoad = j[4][modType]
                            else:
                                for modType in self.modelPriority:
                                    if j[4][modType] != '':
                                        modToLoad = j[4][modType]
                            logging.info(str(modType) + ' ' + str(modToLoad))

                            args = ' '.join([join(self.dataPath, j[0]), join(self.modelPath, modToLoad),
                                             self.interactionConfFile, interactionFunction[0], str(self.windowed)])
                            cmd = 'interactionSAMModel.py ' + args

                            if self.verbose:
                                logging.info('')
                                logging.info("cmd = " + str(cmd))
                                logging.info('')
                            if self.persistence:
                                command = "bash -c \"" + cmd + "; exec bash\""
                            else:
                                command = "bash -c \"" + cmd + "\""

                            if self.windowed:
                                c = subprocess.Popen([self.terminal, '-e', command], shell=False)
                            else:
                                c = subprocess.Popen([cmd], shell=True)

                            self.rpcConnections.append([j[0], interfacePort, interfacePortName[:-1], callSignList, c, 'loading'])
                            # pause here

                            noConn = True
                            iters = 0
                            if self.verbose:
                                logging.info('connecting ' + self.rpcConnections[-1][2]+'o' +
                                             ' with ' + self.rpcConnections[-1][2]+'i')
                            while noConn:
                                try:
                                    noConn = yarp.Network.connect(self.rpcConnections[-1][2]+'o',
                                                                  self.rpcConnections[-1][2]+'i')
                                except:
                                    noConn = False

                                noConn = not noConn
                                time.sleep(1)
                                iters += 1
                                if iters >= 10:
                                    break

                            if noConn:
                                reply.addString('nack')
                                reply.addString("Failure to load " + str(interactionFunction[0]) + " model")
                                rep = yarp.Bottle()
                                cmd = yarp.Bottle()
                                cmd.addString("close")
                                cmd.addString(self.rpcConnections[-1][0])
                                self.closeModel(rep, cmd)
                            else:
                                # then execute an interaction model check to verify correct startup
                                logging.info('pinging portNames to ' + str(self.rpcConnections[-1][0]))
                                rep = yarp.Bottle()
                                cmd = yarp.Bottle()
                                cmd.addString("portNames")
                                self.rpcConnections[-1][1].write(cmd, rep)
                                self.rpcConnections[-1][-1] = 'ready'
                                logging.info('ping received ' + str(rep.toString()))

                                if self.rpcConnections[-1][0] not in self.modelConnections.keys():
                                    self.modelConnections[self.rpcConnections[-1][0]] = dict()
                                if rep.size() > 1 and rep.get(0).asString() == 'ack':
                                    for p in range(rep.size()):
                                        if rep.get(p).asString() != 'ack':
                                            if rep.get(p).asString() not in self.modelConnections[self.rpcConnections[-1][0]].keys():
                                                self.modelConnections[self.rpcConnections[-1][0]][rep.get(p).asString()] = []
                                                logging.info('Monitoring ' + rep.get(p).asString())
                                            else:
                                                # reinstate previously present connections
                                                for op in self.modelConnections[self.rpcConnections[-1][0]][rep.get(p).asString()]:
                                                    if op[1] == 'in':
                                                        logging.info('Connect ' + str(op[0]) + ' to ' + rep.get(p).asString())
                                                        yarp.Network.connect(op[0], rep.get(p).asString())
                                                    else:
                                                        logging.info('Connect ' + rep.get(p).asString() + ' to ' + str(op[0]))
                                                        yarp.Network.connect(rep.get(p).asString(), op[0])
                                reply.addString('ack')
                                reply.addString(str(interactionFunction[0]) + " model loaded at " +
                                                interfacePortName + " with call signs " + str(callSignList))
                    else:
                        reply.addString('nack')
                        reply.addString('No interaction function found in ' + command.get(1).asString() +
                                        ' model path. Skipping model')
                else:
                    reply.addString('nack')
                    reply.addString('Parameters "driver" and "modelBaseName" not found in config.ini')
            else:
                reply.addString('nack')
                reply.addString("Failed to retrieve " + command.get(1).asString() + " model. Model not trained")
        else:
            reply.addString('nack')
            reply.addString(command.get(1).asString() + " model does not exist")
        del parser

    def checkModel(self, reply, command, allCheck=False):
        """ Check availability and status of a model.

            Description:
                  Cross check available models, data folders, config files and driver availability for a specific model.

            Args:
                reply   : Yarp bottle for the reply from the function.
                command : Yarp bottle with the command and the model name to check.
                allCheck : Boolean to check all models if True and check a single model if False.

            Returns :
                Boolean indicating success or not.
        """
        reply.clear()
        # update to show which models are loaded or not and which are currently training
        repStr = ''
        if command.size() != 2:
            repStr += "Model name required. e.g. check Actions"
        elif command.get(1).asString() in self.trainingListHandles:
            repStr += command.get(1).asString()+" in training"
        elif command.get(1).asString() in self.uptodateModelsNames:
            repStr += command.get(1).asString()+" is up-to-date"
        elif command.get(1).asString() in self.updateModelsNames:
            repStr += command.get(1).asString()+" requires update"
        elif command.get(1).asString() in self.noModelsNames:
            repStr += command.get(1).asString()+" has no model"
        else:
            repStr += command.get(1).asString()+" model not present"

        if any(e[0] == command.get(1).asString() for e in self.rpcConnections):
            repStr += " and is loaded"
        elif command.get(1).asString() in self.uptodateModelsNames + self.updateModelsNames:
            repStr += " and is not loaded"
        if not allCheck:
            reply.addString('ack')
        reply.addString(repStr)
        return True
        
    def train(self, reply, command):
        """ Logic for training a model.

            Description:
                  Checks that requested model is present and available for training.

            Args:
                reply   : Yarp bottle for the reply from the function.
                command : Yarp bottle with the command and the model name to train.

            Returns :
                Boolean indicating success or not.
        """
        reply.clear()
        
        if command.size() != 2:
            reply.addString('nack')
            reply.addString("Model name required. e.g. train Actions")
        elif str(command.get(1).asString()) in self.uptodateModelsNames:
            reply.addString('nack')
            reply.addString(command.get(1).asString() + " is already up to date.")
        elif str(command.get(1).asString()) in self.trainingListHandles:
            reply.addString('nack')
            reply.addString(command.get(1).asString() + " is already being trained.")
        elif command.get(1).asString() in self.updateModelsNames or command.get(1).asString() in self.noModelsNames:
            reply.addString('ack')
            reply.addString("Training " + command.get(1).asString() + " model ...")
            modelToTrain = [s for s in self.updateModels + self.noModels if s[0] == command.get(1).asString()][0]
            if self.verbose:
                logging.info(modelToTrain)
            self.train_model(modelToTrain)
        else:
            reply.addString('nack')
            reply.addString(command.get(1).asString() + " model not available to train")
        
        return True

    def optimise(self, reply, command):
        """ Logic for optimising a model.

            Description:
                  Checks that requested model is present and available for optimisation.

            Args:
                reply   : Yarp bottle for the reply from the function.
                command : Yarp bottle with the command and the model name to optimise.

            Returns :
                Boolean indicating success or not.
        """
        reply.clear()
        
        if command.size() < 2:
            reply.addString('nack')
            reply.addString("Model name required. e.g. optimise Actions")
        elif str(command.get(1).asString()) in self.trainingListHandles:
            reply.addString('nack')
            reply.addString(command.get(1).asString() + " is already being trained.")
        elif command.get(1).asString() in self.noModelsNames:
            reply.addString('nack')
            reply.addString("Train " + command.get(1).asString() + " model before optimising")
        elif command.get(1).asString() in self.updateModelsNames or \
                command.get(1).asString() in self.uptodateModelsNames:
            reply.addString('ack')
            reply.addString("Optimising " + command.get(1).asString() + " model ...")
            modelToTrain = [s for s in self.updateModels + self.uptodateModels if s[0] == command.get(1).asString()][0]
            if self.verbose:
                logging.info(modelToTrain)
            self.optimise_model(modelToTrain, modName=command.get(2).asString())
        else:
            reply.addString('nack')
            reply.addString(command.get(1).asString() + " model not available to optimise")
        
        return True

    def deleteModel(self, reply, command):
        """ Deletes model from hard disk.

            Args:
                reply   : Yarp bottle for the reply from the function.
                command : Yarp bottle with the command and the model name to delete.

            Returns :
                Boolean indicating success or not.
        """
        reply.clear()
        b = yarp.Bottle()
        self.checkAvailabilities(b)
        alreadyOpen = False
        for k in self.rpcConnections:
            if k[0] == command.get(1).asString():
                alreadyOpen = True
        
        if command.size() < 2:
            reply.addString('nack')
            reply.addString("Model name required. e.g. delete Actions")
        elif command.get(1).asString() in self.trainingListHandles:
            reply.addString('nack')
            reply.addString("Cannot delete model. Model in training")
        elif alreadyOpen:
            reply.addString('nack')
            reply.addString("Cannot delete model. Model currently loaded")
        elif command.get(1).asString() in self.updateModelsNames or \
                command.get(1).asString() in self.uptodateModelsNames:

            modelToDelete = [s for s in self.updateModels + self.uptodateModels
                             if s[0] == command.get(1).asString()][0][4]

            for j in modelToDelete.keys():
                if 'L' in modelToDelete[j].split('__')[-1]:
                    modelToDelete[j] = '__'.join(modelToDelete.split('__')[:-1])

            filesToDelete = []
            if command.get(2).asString() != '':
                modName = command.get(2).asString()
                if modName in modelToDelete.keys():
                    if modelToDelete[modName] != '':
                        filesToDelete += glob.glob(join(self.modelPath, modelToDelete[modName] + '*'))
            else:
                for j in modelToDelete.keys():
                    if modelToDelete[j] != '':
                        filesToDelete += glob.glob(join(self.modelPath, modelToDelete[j] + '*'))
            logging.info(filesToDelete)
            failFlag = False
            for i in filesToDelete:
                try:
                    os.remove(i)
                except:
                    failFlag = True

            if len(filesToDelete) > 0 and not failFlag:
                reply.addString('ack')
                reply.addString(str(command.get(1).asString()) + " model deleted.")
            elif len(filesToDelete) == 0:
                reply.addString('nack')
                reply.addString('Model name with ' + command.get(2).asString() + ' not found')
            elif failFlag:
                reply.addString('nack')
                reply.addString('Error when deleting file')

            b = yarp.Bottle()
            self.checkAvailabilities(b)
        else:
            reply.addString('nack')
            reply.addString(str(command.get(1).asString()) + " model not present")
        return True

    def configModel(self, reply, command):
        """ Displays the model configuration file using a system call to gedit.

            Args:
                reply   : Yarp bottle for the reply from the function.
                command : Yarp bottle with the command and the model name to display.

            Returns :
                Boolean indicating success or not.
        """
        reply.clear()
        if command.size() != 2:
            reply.addString('nack')
            reply.addString("Model name required. e.g. report Actions")
        elif command.get(1).asString() in self.updateModelsNames or \
                        command.get(1).asString() in self.uptodateModelsNames or \
                        command.get(1).asString() in self.noModelsNames:
            modelToCheck = [s for s in self.updateModels + self.uptodateModels + self.noModels
                            if s[0] == command.get(1).asString()][0][0]

            logging.info(modelToCheck)

            modelConfFile = join(self.dataPath, modelToCheck, 'config.ini')
            os.system("gedit " + modelConfFile)
            reply.addString('ack')
            reply.addString(modelToCheck)
        else:
            reply.addString('nack')
        return True

    def dataDirModel(self, reply, command):
        """ Returns the chosen model

            Description:
                samOptimiser creates multiple models with different suffixes. `best` indicates the best performing model. `exp` indicates the last trained model. `backup` indicates the model present before optimisation started. The priority of suffixes is `best`, `exp`, `backup`.

            Args:
                reply   : Yarp bottle for the reply from the function.
                command : Yarp bottle with the command and the model name to chose.

            Returns :
                Boolean indicating success or not.
        """
        reply.clear()
        if command.size() < 2:
            reply.addString('nack')
            reply.addString("Model name required. e.g. dataDir Actions")
        elif command.get(1).asString() in self.updateModelsNames or \
                        command.get(1).asString() in self.uptodateModelsNames:

            modelToCheck = [s for s in self.updateModels + self.uptodateModels
                             if s[0] == command.get(1).asString()][0][4]

            modType = command.get(2).asString()
            modToLoad = ''
            if modType != '':
                if modType in modelToCheck.keys():
                    if modelToCheck[modType] != '':
                        modToLoad = modelToCheck[modType]
            else:
                for modType in self.modelPriority:
                    if modelToCheck[modType] != '':
                        modToLoad = modelToCheck[modType]

            if modToLoad != '':
                modToLoad = join(self.modelPath, modToLoad) + '.pickle'
                logging.info(modToLoad)
                reply.addString('ack')
                reply.addString(modToLoad)
            else:
                reply.addString('nack')
                reply.addString('Could not find model type ' + modType)
        else:
            reply.addString('nack')
            reply.addString('Could not find ' + command.get(1).asString())

        return True

    def reportModel(self, reply, command):
        """ Returns the performance of the trained model.

            Description:
                Returns the confusion matrix for a trained model or returns the confusion matrix together with a plot of the matrix.

            Args:
                reply   : Yarp bottle for the reply from the function.
                command : Yarp bottle with the command, the model name to report and a plot parameter to switch plot on or off.

            Examples:
                report <modelName> \n
                report <modelName> plot

            Returns :
                Boolean indicating success or not.
        """
        reply.clear()

        if command.size() < 2:
            reply.addString('nack')
            reply.addString("Model name required. e.g. report Actions")
        elif command.get(1).asString() in self.updateModelsNames or \
             command.get(1).asString() in self.uptodateModelsNames:

            plotFlag = False
            if command.size() == 3 and command.get(2).asString() == 'plot':
                plotFlag = True

            modelToCheck = [s for s in self.updateModels + self.uptodateModels
                             if s[0] == command.get(1).asString()][0][4]

            for j in modelToCheck.keys():
                if 'L' in modelToCheck[j].split('__')[-1]:
                    modelToCheck[j] = '__'.join(modelToCheck.split('__')[:-1])

            reply.addVocab(yarp.Vocab_encode("many"))
            filesToCheck = []
            for j in modelToCheck.keys():
                if modelToCheck[j] != '':
                    filesToCheck = glob.glob(join(self.modelPath, modelToCheck[j] + '*'))
                    for i in filesToCheck:
                        if '.pickle' in i:
                            modelPickle = pickle.load(open(i, 'rb'))
                            reply.addString('ack')
                            reply.addString(modelToCheck[j]+":")
                            try:
                                perfLabels = modelPickle['overallPerformanceLabels']
                            except:
                                perfLabels = None
                            perfCM = modelPickle['overallPerformance']

                            if plotFlag:
                                utils.plot_confusion_matrix(perfCM, perfLabels, title=modelToCheck[j])

                            if perfLabels is not None:
                                reply.addString(str(perfLabels))
                            reply.addString(str(perfCM))
                            reply.addString("\t"+"  ")
        else:
            reply.addString('nack')
            reply.addString(str(command.get(1).asString()) + " model not present")
        return True

    def train_model(self, mod):
        """ Train a model.

            Description:
                  Launches trainSAMModel.py to train the specified model.

            Args:
                mod   : String with the model name to load.

            Returns :
                Boolean indicating success or not.
        """
        if self.verbose:
            logging.info("Training Models:")
            logging.info('')

        # OLD
        # n = mod[1] + '.py'
        # trainPath = join(self.trainingFunctionsPath, n)
        # NEW
        trainPath = 'trainSAMModel.py'

        if self.verbose:
            logging.info('Training ' + mod[0] + ' ...')
            logging.info('Opening ' + trainPath)
            logging.info('')
        dPath = join(self.dataPath, mod[0])

        if mod[4] != '':
            modToTrain = mod[4]['exp']
        else:
            modToTrain = ''

        if modToTrain != '':
            mPath = join(self.modelPath, modToTrain) + '.pickle'
        else:
            mPath = join(self.modelPath, modToTrain)

        if self.verbose:
            logging.info(mPath)

        # #open separate ipython for training
        # #this will allow separate training across different computers in future
        # OLD
        # if(mod[0] in self.updateModelsNames):
        #      args = ' '.join([dPath, mPath, mod[1], 'update'])
        # else:
        #      args = ' '.join([dPath, mPath, mod[1], 'new'])
        # NEW
        if mod[0] in self.updateModelsNames:
            args = ' '.join([dPath, mPath, mod[1], 'update', mod[0], str(self.windowed)])
        else:
            args = ' '.join([dPath, mPath, mod[1], 'new', mod[0], str(self.windowed)])

        if self.verbose:
            logging.info('args: ' + str(args))

        # OLD
        # cmd = 'ipython ' + trainPath + ' -- ' + args
        # NEW
        cmd = trainPath + ' ' + args
        if self.persistence:
            command = "bash -c \"" + cmd + "; exec bash\""
        else:
            command = "bash -c \"" + cmd + "\""
        
        if self.verbose:
            logging.info('cmd: ' + str(cmd))

        if self.windowed:
            c = subprocess.Popen([self.terminal, '-e', command], shell=False)
        else:
            # c = subprocess.Popen([cmd], shell=True, stdout=self.devnull, stderr=self.devnull)
            c = subprocess.Popen([cmd], shell=True)
        
        self.trainingListHandles[mod[0]] = c

        return True

    def optimise_model(self, mod, modName):
        """ Optimise a model.

            Description:
                  Launches samOptimiser.py to optimise the specified model.

            Args:
                mod     : Model information.
                modName : Model name.

            Returns :
                Boolean indicating success or not.
        """
        if self.verbose:
            logging.info("Training Models:")
            logging.info('')

        # OLD
        # n = mod[1] + '.py'
        # trainPath = join(self.trainingFunctionsPath, n) 
        # NEW
        trainPath = 'trainSAMModel.py'

        if self.verbose:
            logging.info('Optimising ' + mod[0] + ' ...')
            logging.info('')
        dPath = join(self.dataPath, mod[0])

        modToUse = ''
        if modName != '' and modName in mod[4].keys():
            if mod[4][modName] != '':
                modToUse = mod[4][modName]
        else:
            for nm in self.modelPriority:
                if mod[4][nm] != '':
                    modToUse = mod[4][nm]

        if modToUse != '':
            mPath = join(self.modelPath, modToUse) + '.pickle'
        else:
            mPath = join(self.modelPath, modToUse)

        if self.verbose:
            logging.info(mPath)

        # #open separate ipython for training
        # #this will allow separate training across different computers in future
        # OLD
        # args = ' '.join([dPath, mPath, mod[1], 'new', 'False', 'False', 'True'])
        # NEW
        args = ' '.join([dPath, mPath, mod[1], 'new', mod[0], 'False', 'False', 'True'])
        
        if self.verbose:
            logging.info('args: ' + str(args))

        cmd = 'samOptimiser.py ' + trainPath + ' ' + args
        if self.persistence:
            command = "bash -c \"" + cmd + "; exec bash\""
        else:
            command = "bash -c \"" + cmd + "\""
        
        if self.verbose:
            logging.info('cmd: ' + str(cmd))

        if self.windowed:
            c = subprocess.Popen([self.terminal, '-e', command], shell=False)
        else:
            c = subprocess.Popen([cmd], shell=True)

        self.trainingListHandles[mod[0]] = c

        return True

    def getPeriod(self):
        """
           Module refresh rate.

           Returns : The period of the module in seconds.
        """
        return 0.1

    def onlineModelCheck(self):
        """ Check status of loaded models.

            Description:
                  Checks that ports for all loaded models are alive and working. If a loaded model is not working restart the model.

            Args:
                None
            Returns :
                None
        """
        # check communication with loaded models
        readyList = []
        for i, v in self.trainingListHandles.iteritems():
            if i != 'Cluster':
                ret = v.poll()
                if ret is not None:
                    if ret == 0:
                        readyList += [i]
                        logging.info(str(i) + ' terminated successfully')
                        b = yarp.Bottle()
                        self.checkAvailabilities(b)
                    else:
                        readyList += [i]
                        logging.error(str(i) + ' terminated with ' + str(self.SIGNALS_TO_NAMES_DICT[abs(ret)]))
                else:
                    # if(self.verbose): logging.info(i, "still training "
                    pass
        
        for i in readyList:
            del self.trainingListHandles[i]

        for j in range(len(self.rpcConnections)):
            currModelName = copy.deepcopy(self.rpcConnections[j][0])
            correctOp_check1, correctOp_check2 = self.checkOperation(self.rpcConnections[j])
            correctOperation = correctOp_check1 and correctOp_check2
            if not correctOperation and self.rpcConnections[j][-1] != 'loading':
                self.connectionCheckCount = 0
                if self.rpcConnections[j][0] not in self.nonResponsiveDict.keys():
                    self.nonResponsiveDict[self.rpcConnections[j][0]] = 1
                else:
                    self.nonResponsiveDict[self.rpcConnections[j][0]] += 1
                logging.warning(str(self.rpcConnections[j][0]) + ' not responding ' + str(self.nonResponsiveDict[self.rpcConnections[j][0]]) +
                    '/' + str(self.nonResponsiveThreshold))
                if self.nonResponsiveDict[self.rpcConnections[j][0]] >= self.nonResponsiveThreshold:
                    logging.error('Restarting ' + str(self.rpcConnections[j][0]) + ' model')
                    if not correctOp_check1 or currModelName in self.nonResponsiveDict.keys():
                        rep = yarp.Bottle()
                        cmd = yarp.Bottle()
                        cmd.addString("load")
                        cmd.addString(currModelName)
                        self.loadModel(rep, cmd)
                    else:
                        rep = yarp.Bottle()
                        cmd = yarp.Bottle()
                        cmd.addString("close")
                        cmd.addString(self.rpcConnections[j][0])
                        self.closeModel(rep, cmd)
                    # try:
                    logging.info(currModelName)
                    logging.info('')
                    self.nonResponsiveDict[self.rpcConnections[j][0]] = 0
                    # except:
                    #     pass
            else:
                self.nonResponsiveDict[self.rpcConnections[j][0]] = 0
                self.connectionCheckCount += 1
                if self.connectionCheckCount == 10:
                    for n in range(len(self.rpcConnections)):
                        logging.info('ALL KEYS ' + str(self.modelConnections.keys()))
                        if self.rpcConnections[n][0] in self.modelConnections.keys():
                            logging.info(self.rpcConnections[n][0])
                            if len(self.modelConnections[self.rpcConnections[n][0]].keys()) == 0:
                                logging.info('pinging portNames to ' + str(self.rpcConnections[n][0]))
                                rep = yarp.Bottle()
                                cmd = yarp.Bottle()
                                cmd.addString("portNames")
                                self.rpcConnections[n][1].write(cmd, rep)
                                self.rpcConnections[n][-1] = 'ready'
                                logging.info('ping received ' + str(rep.toString()))

                                if rep.size() > 1 and rep.get(0).asString() == 'ack':
                                    for p in range(rep.size()):
                                        if rep.get(p).asString() != 'ack':
                                            if rep.get(p).asString() not in self.modelConnections[self.rpcConnections[n][0]].keys():
                                                self.modelConnections[self.rpcConnections[n][0]][rep.get(p).asString()] = []
                                                logging.info('Monitoring ' + str(rep.get(p).asString()))
                            else:
                                for k in self.modelConnections[self.rpcConnections[n][0]].keys():
                                    logging.info('ping ' + str(k))
                                    proc = subprocess.Popen(['yarp', 'ping', k], stdout=subprocess.PIPE)
                                    output = proc.stdout.read()
                                    proc.wait()
                                    del proc

                                    conStrings = output.split('\n')[1:]
                                    connList = []
                                    for g in conStrings:
                                        if 'output conn' in g:
                                            dirConnect = 'out'
                                        elif 'input conn' in g:
                                            dirConnect = 'in'
                                        else:
                                            dirConnect = None

                                        if 'from' in g and dirConnect is not None:
                                            parts = g.split(' ')
                                            if dirConnect == 'out':
                                                connList.append([parts[8], dirConnect])
                                            elif dirConnect == 'in' and '<ping>' not in g:
                                                connList.append([parts[6], dirConnect])

                                    self.modelConnections[self.rpcConnections[n][0]][k] = connList

                            logging.info(self.modelConnections)
                            logging.info('')
                    self.connectionCheckCount = 0

    def checkOperation(self, j):
        """
            Check heartbeat of model.

            Args:
                j : Yarp rpc port for loaded model to check.

            Returns:
                correctOp_check1 : Check that output count for the model rpc port is > 0.
                correctOp_check2 : Check that the model returns `ack` to a `hartbeat` request.

        """
        correctOp_check1 = True if j[1].getOutputCount() > 0 else False
        rep = yarp.Bottle()
        cmd = yarp.Bottle()
        cmd.addString("heartbeat")
        j[1].write(cmd, rep)
        correctOp_check2 = True if rep.get(0).asString() == 'ack' else False
        # correctOperation = correctOp_check1 and correctOp_check2
        return correctOp_check1, correctOp_check2

    def updateModule(self):
        """
            Logic to execute every getPeriod() seconds.

            Description:
                This function makes checks that all loaded modules are still alive and if OPC querying is enabled query OPC.

            Returns: Boolean indicating success of logic or not.
        """
        if self.iter == 10:
            self.onlineModelCheck()
            if self.useOPC:
                rep = yarp.Bottle()
                self.askOPC(rep)
            self.iter = 0
        self.iter += 1
        time.sleep(0.05)
        return True


def exception_hook(exc_type, exc_value, exc_traceback):
    """Callback function to record any errors that occur in the log files.

        Documentation:
            Substitutes the standard python exception_hook with one that records the error into a log file. Can only work if interactionSAMModel.py is called from python and not ipython because ipython overrides this substitution.
        Args:
            exc_type: Exception Type.
            exc_value: Exception Value.
            exc_traceback: Exception Traceback.

        Returns:
            None
    """
    logging.error("Uncaught exception", exc_info=(exc_type, exc_value, exc_traceback))

sys.excepthook = exception_hook

if __name__ == '__main__':

    plt.ion()
    yarp.Network.init() 
    samMod = SamSupervisorModule()
    yrf = yarp.ResourceFinder()
    yrf.setVerbose(True)
    yrf.setDefaultContext("samSupervisor")
    yrf.setDefaultConfigFile("default.ini")
    yrf.configure(sys.argv)

    samMod.runModule(yrf)
