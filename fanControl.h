#ifndef FANCONTROL_H
#define FANCONTROL_H
#include <list>
#include <string>
#include <map>
#include <fstream>
#include <vector>
#include <iostream>
#include <utility>
#include <nlohmann/json.hpp>
#include <cmath>
#include <deque>

using namespace std;

    class TempSensorServer {
        private:
            list<ifstream> tempSensorStreams;
            vector<pair<string, reference_wrapper<ifstream>>> tempSensor;
            vector<pair<string, string>> tempSensorNamePathCoor;
        protected:

        public:
            TempSensorServer(vector<string> tempPaths, vector<string> sensorName);
            ifstream& getTempSenseIfstream(const string &sensorPath);
    };

    class GetTemperature {
        private:
            vector<int> fanRpm;
            vector<reference_wrapper<ifstream>> tempSensor;
            vector<vector<pair<int, int>>> tempRpmGraph;
            string function;
            vector<int> rpms;
            int maxPwm;
            int avgTimes;
            deque<int> lastPwmValues;
            int averaging(int pwm);
        protected:
            void getRpm();
            int getFanRpm();
        public: 
            GetTemperature(vector<string> tempPath, vector<vector<pair<int, int>>> tempRpmGraph, string function, int maxPwm, int avgTimes, TempSensorServer* tmpSrv); 
            ~GetTemperature();
    };

    class FanControl {
        private:
            string const autoGenFileAppend = "_fanSettings";
            string fanNamePath;
            const string stateFilesPath = "/var/lib/fanCommander/";

            ofstream fanControl;
            ifstream rpmSensor;
            fstream fanSettingsAutoGenFile;
            int minPwmGood;
            int minPwm;
            int startPwmGood;
            int startPwm;
            int maxPwmGood;
            void writeMinStartPwm(fstream &file);
            void getMinStartPwm(fstream &file);
            void waitForFanRpmToStabilize();

            int feedBackRpm;
        public: 
            FanControl(string fanPath, string rpmPath, int minPwm, int maxPwm, int startPwm);
            void setFanSpeed(int pwm);
            void getFeedbackRpm();
            ~FanControl();
    };

    class SetFans : protected FanControl, protected GetTemperature {
        private:
            int fanRpm;
        protected:

        public:
            SetFans(vector<string> tempPath, vector<vector<pair<int, int>>> tempRpmGraph, string function, string fanPath, string rmpPath, int minPwm, int maxPwm, int startPwm, int avgTimes, TempSensorServer *tmpSrv) : 
            FanControl(fanPath, rmpPath, minPwm, maxPwm, startPwm), GetTemperature(tempPath, tempRpmGraph, function, maxPwm, avgTimes, tmpSrv) {

            };
            void declareFanRpmFromTempGraph();
            void setFanSpeedFromDeclaredRpm();
    };

#endif