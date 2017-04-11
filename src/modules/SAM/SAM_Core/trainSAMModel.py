#!/usr/bin/env python
# """"""""""""""""""""""""""""""""""""""""""""""
# The University of Sheffield
# WYSIWYD Project
#
# A generic training class that is used to train all models.
# This class can train both single model implementations as well as multiple model implementations
#
# Created on 20 July 2016
#
# @author: Daniel Camilleri
#
# """"""""""""""""""""""""""""""""""""""""""""""
import warnings
import sys
import numpy
import numpy as np
from SAM.SAM_Core import SAMCore
from SAM.SAM_Core import SAMTesting
from SAM.SAM_Core.SAM_utils import initialiseModels
import logging
import os
from os.path import join
np.set_printoptions(threshold=numpy.nan, precision=2)
warnings.simplefilter("ignore")


def exception_hook(exc_type, exc_value, exc_traceback):
    logging.error("Uncaught exception", exc_info=(exc_type, exc_value, exc_traceback))

sys.excepthook = exception_hook

dataPath = sys.argv[1]
modelPath = sys.argv[2]
driverName = sys.argv[3]
windowedMode = sys.argv[6] == 'True'
baseLogFileName = 'trainErrorLog_' + driverName

file_i = 0
loggerFName = join(dataPath, baseLogFileName + '_' + str(file_i) + '.log')

# check if file exists
while os.path.isfile(loggerFName) and os.path.getsize(loggerFName) > 0:
    loggerFName = join(dataPath, baseLogFileName + '_' + str(file_i) + '.log')
    file_i += 1

if windowedMode:
    logFormatter = logging.Formatter("[%(levelname)s]  %(message)s")
else:
    logFormatter = logging.Formatter("\033[34m%(asctime)s [%(name)-33s] [%(levelname)8s]  %(message)s\033[0m")

rootLogger = logging.getLogger('train ' + driverName)
rootLogger.setLevel(logging.DEBUG)

fileHandler = logging.FileHandler(loggerFName)
fileHandler.setFormatter(logFormatter)
rootLogger.addHandler(fileHandler)

consoleHandler = logging.StreamHandler()
consoleHandler.setFormatter(logFormatter)
rootLogger.addHandler(consoleHandler)
logging.root = rootLogger

logging.info(loggerFName)

mm = initialiseModels(sys.argv[1:4], sys.argv[4])
# mm[0].SAMObject.visualise()

if mm[0].calibrateUnknown or len(mm) > 1:
    SAMTesting.calibrateModelRecall(mm)

overallPerformance = 100000
if mm[0].model_mode != 'temporal':
    overallPerformance, overallPerformanceLabels, labelComparisonDict = mm[0].testPerformance(mm, mm[0].Yall, mm[0].Lall, mm[0].YtestAll, mm[0].LtestAll, True)
elif mm[0].model_mode == 'temporal':
    overallPerformance, overallPerformanceLabels, labelComparisonDict = mm[0].testTemporalPerformance(mm, mm[0].Xall, mm[0].Yall, mm[0].Lall,
                                                       mm[0].XtestAll, mm[0].YtestAll, mm[0].LtestAll, True)

numParts = len(mm[0].participantList)
for k in range(numParts):
    mm[k].paramsDict['overallPerformance'] = overallPerformance
    mm[k].paramsDict['overallPerformanceLabels'] = overallPerformanceLabels
    mm[k].paramsDict['labelComparisonDict'] = labelComparisonDict
    mm[k].paramsDict['ratioData'] = mm[0].ratioData
    mm[k].paramsDict['model_type'] = mm[k].model_type
    mm[k].paramsDict['model_mode'] = mm[0].model_mode
    mm[k].paramsDict['verbose'] = mm[0].verbose
    mm[k].paramsDict['Quser'] = mm[0].Quser
    mm[k].paramsDict['model_num_inducing'] = mm[0].model_num_inducing
    mm[k].paramsDict['model_num_iterations'] = mm[0].model_num_iterations
    mm[k].paramsDict['model_init_iterations'] = mm[0].model_init_iterations
    mm[k].paramsDict['kernelString'] = mm[0].kernelString
    mm[k].paramsDict['economy_save'] = mm[0].economy_save

    if mm[0].model_mode != 'temporal':
        mm[k].paramsDict['textLabels'] = mm[0].textLabels
        mm[k].paramsDict['modelQ'] = mm[0].SAMObject.Q
    elif mm[0].model_mode == 'temporal':
        mm[k].paramsDict['temporalModelWindowSize'] = mm[0].temporalModelWindowSize

    if mm[k].model_type == 'mrd' and mm[k].model_mode != 'temporal':
        logging.info(mm[k].Y['L'].shape)
        logging.info(mm[k].Y['Y'].shape)

    if k == 0:
        mm[0].paramsDict['listOfModels'] = mm[0].listOfModels
        mm[0].paramsDict['avgClassTime'] = mm[0].avgClassTime
        mm[0].paramsDict['optimiseRecall'] = mm[0].optimiseRecall
        if mm[0].calibrateUnknown:
            mm[0].paramsDict['classificationDict'] = mm[0].classificationDict
        mm[0].paramsDict['calibrateUnknown'] = mm[0].calibrateUnknown
        mm[0].paramsDict['calibrated'] = mm[0].calibrated
        if numParts == 1:
            if mm[0].X is None:
                mm[0].paramsDict['X'] = mm[0].X
            else:
                mm[0].paramsDict['X'] = mm[0].X.shape

            if mm[0].model_mode != 'temporal':
                mm[0].paramsDict['Y'] = mm[k].Y['Y'].shape

            mm[0].paramsDict['useMaxDistance'] = mm[0].useMaxDistance

    elif numParts > 1:
        # fname = mm[0].listOfModels[k-1]
        if mm[k].X is None:
            mm[k].paramsDict['X'] = mm[k].X
        else:
            mm[k].paramsDict['X'] = mm[k].X.shape
        mm[k].paramsDict['Y'] = mm[k].Y['Y'].shape
    # else:
    #     pass
        # fname = fnameProto

    # save model with custom .pickle dictionary by iterating through all nested models
    logging.info('-------------------')
    logging.info('Saving: ' + mm[k].fname)
    mm[k].saveParameters()
    logging.info('Keys:')
    logging.info(mm[k].paramsDict.keys())
    SAMCore.save_pruned_model(mm[k].SAMObject, mm[k].fname, mm[0].economy_save, extraDict=mm[k].paramsDict)
