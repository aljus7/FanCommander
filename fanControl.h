#ifndef FANCONTROL_H
#define FANCONTROL_H
#include <string>
#include <map>
#include <fstream>
#include <vector>
#include <iostream>
#include <utility>
#include <nlohmann/json.hpp>
#include <cmath>

using namespace std;


    class GetTemperature {
        private:
            vector<int> fanRpm;
            vector<ifstream> tempSensor;
            vector<vector<pair<int, int>>> tempRpmGraph;
            string function;
            vector<int> rpms;
            int maxPwm;
            int avgTimes;
            vector<int> lastPwmValues;
            int averaging(int pwm);
        protected:
            void getRpm();
            int getFanRpm();
        public: 
            GetTemperature(vector<string> tempPath, vector<vector<pair<int, int>>> tempRpmGraph, string function, int maxPwm, vector<int> maxTemp, int avgTimes); 
    };

    class FanControl {
        private:
            string autoGenFileName = "fanSettingsAutoGenFile";
            ofstream fanControl;
            ifstream rpmSensor;
            fstream fanSettingsAutoGenFile;
            int minPwmGood;
            int startPwmGood;
            void writeMinStartPwm(fstream &file);
            void waitForFanRpmToStabilize();
        protected:

        public: 
            FanControl(string fanPath, string rpmPath, int minPwm, int maxPwm, int startPwm);
    };

    class SetFans : protected FanControl, protected GetTemperature {
        private:

        protected:

        public:
            SetFans(vector<string> tempPath, vector<vector<pair<int, int>>> tempRpmGraph, string function, string fanPath, string rmpPath, int minPwm, int maxPwm, vector<int> maxTempGraph, int startPwm, int avgTimes) : 
            FanControl(fanPath, rmpPath, minPwm, maxPwm, startPwm), GetTemperature(tempPath, tempRpmGraph, function, maxPwm, maxTempGraph, avgTimes) {

            }
    };

#endif