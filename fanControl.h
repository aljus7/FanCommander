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
            GetTemperature(vector<string> tempPath, vector<vector<pair<int, int>>> tempRpmGraph, string function, int maxPwm, int avgTimes); 
            ~GetTemperature();
    };

    class FanControl {
        private:
            string const autoGenFileName = "_settings";
            string fanNamePath;
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
            SetFans(vector<string> tempPath, vector<vector<pair<int, int>>> tempRpmGraph, string function, string fanPath, string rmpPath, int minPwm, int maxPwm, int startPwm, int avgTimes) : 
            FanControl(fanPath, rmpPath, minPwm, maxPwm, startPwm), GetTemperature(tempPath, tempRpmGraph, function, maxPwm, avgTimes) {

            };
            void declareFanRpmFromTempGraph();
            void setFanSpeedFromDeclaredRpm();
    };

#endif