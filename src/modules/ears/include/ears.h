#include "icubclient/clients/icubClient.h"

using namespace std;
using namespace yarp::os;
using namespace icubclient;

class ears : public RFModule {
protected:
    ICubClient *iCub;
    double      period;
    Port        rpc;
    Port        portToBehavior;
    Port        portToSpeechRecognizer;
    BufferedPort<Bottle>        portTarget;
    bool onPlannerMode;
    Mutex m;
    bool bShouldListen;

    std::string      MainGrammar;

public:
    bool configure(yarp::os::ResourceFinder &rf);

    bool interruptModule();

    bool close();

    double getPeriod()
    {
        return period;
    }

    bool    updateModule();
    bool    populateSpecific1(Bottle bInput);

    bool    addUnknownEntity(Bottle bInput);
    bool    setSaliencyEntity(Bottle bInput);

    bool    populateABM(Bottle bInput);


    //RPC & scenarios
    bool respond(const Bottle& cmd, Bottle& reply);
};
