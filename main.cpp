#include "readJson.h"
#include "fanControl.h"
#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>
using namespace std;
const string jsonConfigLocation = "/etc/fanCommander/config.json";

atomic<bool> keepRunning(true);

void signalHandler(int signum) {
    cout << "\nInterrupt signal (" << signum << ") received.\n";
    keepRunning = false;
}

int main() {
    signal(SIGINT, signalHandler);  // Handle Ctrl+C
    signal(SIGTERM, signalHandler); // Handle kill/pkill

    SoftwareParam *softwareParam = new SoftwareParam();
    FanControlParam *fanControlParam = new FanControlParam();

    JsonConfigReader *jsonConfigReader = new JsonConfigReader(jsonConfigLocation);
    jsonConfigReader->readJsonConfig();
    jsonConfigReader->returnJsonConfig(fanControlParam, softwareParam);
    jsonConfigReader->printParsedJsonInStdout(fanControlParam, softwareParam);
    
    vector<SetFans*> setFans;
    vector<string> fanModePaths;

    TempSensorServer *senServ = new TempSensorServer(fanControlParam->tempPaths, fanControlParam->sensorNames);

    for (int i = 0; i < fanControlParam->fanControlPaths.size(); i++) {
        
        vector<string> buildTempTempPaths;
        vector<vector<pair<int, int>>> buildTempTempRpmGraphs;

        fanModePaths.push_back(fanControlParam->fanControlPaths[i] + "_enable");
        ofstream modeFile(fanControlParam->fanControlPaths[i] + "_enable");

        if (modeFile.is_open()) {
            modeFile.seekp(0);
            modeFile << 1 << endl;
            modeFile.close();
        } else {
            throw std::runtime_error("Failed to open fan mode file: \"" + fanControlParam->fanControlPaths[i] + "_enable\"");
        }

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
        fanControlParam->minPwms[i], fanControlParam->maxPwms[i], fanControlParam->startPwms[i], fanControlParam->avgTimes[i], senServ));
    }

    int refreshTime = softwareParam->refreshInterval;

    while (keepRunning) {
        for (auto &fan : setFans) {
            fan->declareFanRpmFromTempGraph();
            fan->setFanSpeedFromDeclaredRpm();
        }
        this_thread::sleep_for(std::chrono::milliseconds(refreshTime));
    }

    std::cout << "Exiting...\n";
    
    // set fan mode to 2 which means automatic bios control
    for (const auto &fanModePath : fanModePaths) {

        ofstream modeFile(fanModePath);

        if (modeFile.is_open()) {
            modeFile.seekp(0);
            modeFile << 2 << endl;
            modeFile.close();
        } else {
            throw std::runtime_error("Failed to open fan mode file.");
        }

    }

    return 0;

}