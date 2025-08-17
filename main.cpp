#include "readJson.h"
#include "fanControl.h"
#include <chrono>
#include <thread>
using namespace std;
const string jsonConfigLocation = "config.json";

int main() {

    SoftwareParam *softwareParam = new SoftwareParam();
    FanControlParam *fanControlParam = new FanControlParam();

    JsonConfigReader *jsonConfigReader = new JsonConfigReader(jsonConfigLocation);
    jsonConfigReader->readJsonConfig();
    jsonConfigReader->returnJsonConfig(fanControlParam, softwareParam);
    jsonConfigReader->printParsedJsonInStdout(fanControlParam, softwareParam);
    
    vector<SetFans*> setFans;

    for (int i = 0; i < fanControlParam->fanControlPaths.size(); i++) {
        
        vector<string> buildTempTempPaths;
        vector<vector<pair<int, int>>> buildTempTempRpmGraphs;

        for (int j = 0; j < fanControlParam->sensors[i].size(); j++) {
            string sensorName = fanControlParam->sensors[i][j];
            for (int k = 0; k < fanControlParam->sensorNames.size(); k++) {
                if (sensorName == fanControlParam->sensorNames[k]) {
                    buildTempTempPaths.push_back(fanControlParam->tempPaths[k]);
                    buildTempTempRpmGraphs.push_back(fanControlParam->tempRpmGraphs[k]);
                }
            }
        }

        setFans.push_back(new SetFans(buildTempTempPaths, buildTempTempRpmGraphs, fanControlParam->sensorFunctions[i], fanControlParam->fanControlPaths[i], fanControlParam->fanRpmPaths[i], 
        fanControlParam->minPwms[i], fanControlParam->maxPwms[i], fanControlParam->startPwms[i], fanControlParam->avgTimes[i]));
    }

    int refreshTime = softwareParam->refreshInterval;

    for (auto &fan : setFans) {
        fan->declareFanRpmFromTempGraph();
        fan->setFanSpeedFromDeclaredRpm();
        this_thread::sleep_for(std::chrono::seconds(refreshTime));
    }
}