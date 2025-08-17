#include"fanControl.h"
#include <string>
#include <thread>
using json = nlohmann::json;

GetTemperature::GetTemperature(vector<string> tempPath, vector<vector<pair<int, int>>> tempRpmGraph, string function, int maxPwm, int avgTimes) {
    if (tempPath.size() == tempRpmGraph.size()) {    
        this->tempSensor.resize(tempPath.size());
        for (int i = 0; i < tempPath.size(); i++) {
            if (tempPath[i] != "null" && !tempPath[i].empty())
                this->tempSensor[i].open(tempPath[i]);
        }

        if (!tempRpmGraph.empty()) {
            for (int i = 0; i < tempRpmGraph.size(); i++) {
                if (tempRpmGraph[i].size() >= 3) {
                    this->tempRpmGraph = tempRpmGraph;
                    cout << "Sucessfull tempRpmGraph size validation" << endl;
                } else {
                    cerr << "Only min 3 temp-rpm points permited!" << endl;
                }
            }
        }
        
    } else {
        cerr << "for every temp path needs to be one tempRpm graph, max 4 sensors per fan permited!" << endl;
    }

    if(function == "max" || function == "min" || function == "avg") {
        this->function = function;
    } else {
        cerr << "function can be only of 'min', 'max', 'avg' value (defaulting to max)" << endl;
        this->function = "max";
    }

    if(maxPwm <= 255 && maxPwm > 0) {
        this->maxPwm = maxPwm;
    } else {
        cerr << "maxPwm was set over 255 value or under 1 value. (defaulting to 255)" << endl;
        this->maxPwm = 255;
    }

    this->avgTimes = avgTimes;

}

int GetTemperature::averaging(int pwm) {

    int sum = 0;

    if(this->lastPwmValues.size() > 1) {
        if (this->lastPwmValues.size() < this->avgTimes) {
            this->lastPwmValues[lastPwmValues.size()] = pwm;
        } else {
            if (this->lastPwmValues.size() == this->avgTimes) {
                this->lastPwmValues.erase(this->lastPwmValues.begin());
            }
            this->lastPwmValues.push_back(pwm);
        }

        for (int i = 0; i < this->lastPwmValues.size(); i++) {
            sum += lastPwmValues[i];
        }
        return sum/lastPwmValues.size();
    } else if (this->lastPwmValues.size() == 1) {
        return this->lastPwmValues[0];
    } else {
        cout << "Averafing error, averaging array is not populated yet." << endl;
        return 255;
    }

}

void GetTemperature::getRpm() {
    vector<double> temps;
    string tempStr;
    for(int i = 0; i < this->tempSensor.size(); i++) {
        this->tempSensor[i].seekg(0);
        if (getline(this->tempSensor[i], tempStr)) {
            try {
                temps[i] = std::stod(tempStr)/100;
            } catch (const std::invalid_argument& e) {
                std::cerr << "Invalid argument: " << e.what() << std::endl;
            } catch (const std::out_of_range& e) {
                std::cerr << "Out of range: " << e.what() << std::endl;
            }
        } else {
            cerr << "Failed to read line " << i << endl;
        }

    }

    for (int j = 0; j < temps.size(); j++) {
        // first = temp , second = rpm;
        const int &temp = temps[j];
        vector<pair<int, int>> currGraph = tempRpmGraph[j];

        for (int i = 0; i < currGraph.size(); i++) {
            if (i == 0 && temp <= currGraph[i].first) { // bottom shelf.
                this->rpms[j] = currGraph[i].second;
                break;
            } else {
                if (temp <= currGraph[i].first)
                    this->rpms[j] = currGraph[i-1].second + ((temp - currGraph[i-1].first)*(currGraph[i].second-currGraph[i-1].second))/(currGraph[i].first-currGraph[i-1].first);
            }
        }
    }

}

int GetTemperature::getFanRpm() {
    if(this->rpms.size() > 1) { 

        if (this->function == "max" && !this->rpms.empty()) {
            int max = rpms[0];
            for (int i = 1; i < this->rpms.size(); i++) {
                    max = max > rpms[i] ? max : rpms[i];
            }
            return averaging(max);
        }

        else if (this->function == "min" && !this->rpms.empty()) {
            int min = rpms[0];
            for (int i = 1; i < this->rpms.size(); i++) {
                    min = min < rpms[i] ? min : rpms[i];
            }
            return averaging(min);
        }

        else if (this->function == "avg" && !this->rpms.empty()) {
            int avg = rpms[0];
            int sum = 0;
            for (int i = 0; i < this->rpms.size(); i++) {
                sum += rpms[i];
            }
            avg = sum/rpms.size();
            return averaging(avg);
        }

        else {
            cerr << "function value needs to be either 'max', 'min' or 'avg'" << endl;
            return 255;
        }
        
    } else {
        return averaging(this->rpms[0]);
    }
}

FanControl::FanControl(string fanPath, string rpmPath, int minPwm, int maxPwm, int startPwm) {

    if (!fanPath.empty() && !rpmPath.empty()) {
        this->fanControl.open(fanPath);
        if(fanControl.is_open()) {
            cout << "Fan control: " << fanPath << " successfully open!" << endl;
        } else {
            throw std::runtime_error("Failed to open fan control file.");
        }
        this->rpmSensor.open(rpmPath);
        if (rpmSensor.is_open()) {
            cout << "Fan sensor: " << rpmPath << " sucessfully open!" << endl;
        } else {
            throw std::runtime_error("Failed to open rpm sensor file.");
        }
    }

    if (minPwm >= 0 && minPwm <= 255 && startPwm >= 0 && startPwm <= 255 && maxPwm <= 255 && maxPwm >= 0 && maxPwm != minPwm && startPwm != maxPwm) {
        this->minPwm = minPwm;
        this->startPwm = startPwm;
        this->maxPwmGood = maxPwm;
    } else {
        cout << "ATENTION: Some of the values of min/start/max pwm do not match requirements! Default sane values used. Some of the values will be calculated." << endl;
        this->minPwm = 0;
        this->startPwm = 0;
        this->maxPwmGood = 255;
    }

    ifstream checkFile(this->autoGenFileName);
    if (!checkFile.good()) {
        
        this->fanSettingsAutoGenFile.open(this->autoGenFileName, ios::out | ios::in);
        getMinStartPwm(fanSettingsAutoGenFile);
        this->fanSettingsAutoGenFile.close();

    } else {

        this->fanSettingsAutoGenFile.open(this->autoGenFileName, ios::out | ios::in);
        writeMinStartPwm(fanSettingsAutoGenFile);
        this->fanSettingsAutoGenFile.close();

    }

}

void FanControl::getMinStartPwm(fstream &file) {
    string savedSettingsJson;
    if (file.is_open()) {
        string line;
        while (getline (file, line)) {
            savedSettingsJson += line;
        }
        json savedVal = json::parse(savedSettingsJson);
        this->minPwmGood = savedVal["minPwm"].get<int>();
        this->startPwmGood = savedVal["startPwm"].get<int>();
        this->maxPwmGood = savedVal["maxPwm"].get<int>();
    } else {
        std::cerr << "File is not open!" << std::endl;
        throw std::runtime_error("Failed to open saved values file.");
    }
}

void FanControl::writeMinStartPwm(fstream &file) {
    
    // calculating min pwm
    this->fanControl.seekp(0);
    this->fanControl << 255 << endl;
    waitForFanRpmToStabilize();
    for (int i = 255; i >= 0; i--) {
        this->fanControl.seekp(0);
        this->fanControl << i << endl;
        waitForFanRpmToStabilize();
        string rpm;
        this->rpmSensor.seekg(0);
        if (getline(this->rpmSensor, rpm)) {
            if (stod(rpm) == 0) {
                this->minPwmGood = i;
                break;
            }
        }
    }

    // calculationg start pwm
    for (int i = 0; i < 255; i++) {
        this->fanControl.seekp(0);
        this->fanControl << i << endl;
        waitForFanRpmToStabilize();
        string rpm;
        this->rpmSensor.seekg(0);
        if (getline(this->rpmSensor, rpm)) {
            if (stod(rpm) > 0) {
                this->startPwmGood = i;
                break;
            }
        }
    }

    if (this->startPwm > this->startPwmGood) {
        cout << "Custom StartPwm value set that is higher than real one, using custiom value" << endl;
        this->startPwmGood = this->startPwm;
    }

    if (this->minPwm > this->minPwmGood) {
        cout << "Custom MinPwm value set that is higher than real one, using custiom value" << endl;
        this->minPwmGood = this->minPwm;
    }

    if(file.is_open()) {
        json jObject;
        jObject["minPwm"] = this->minPwmGood;
        jObject["startPwm"] = this->startPwmGood;
        jObject["maxPwm"] = this->maxPwmGood;

        file << jObject.dump(4);
    } else {
        std::cerr << "File is not open!" << std::endl;
        throw std::runtime_error("Failed to open saved values file.");
    }
        
}

void FanControl::waitForFanRpmToStabilize() {
    string fanRpmStr;
    int prevRpm = 256;
    int diff;
    int i = 0;
    do {
        this->rpmSensor.seekg(0);
        if (getline(this->rpmSensor, fanRpmStr)) {
            int fanRpm = stod(fanRpmStr);
            if (prevRpm == 256) {    
                prevRpm = fanRpm;
                this_thread::sleep_for(std::chrono::milliseconds(1000));
                diff = abs(prevRpm-fanRpm);
            } else {
                this_thread::sleep_for(std::chrono::milliseconds(1000));
                diff = abs(prevRpm-fanRpm);
            }
            ++i;
        }
    } while(diff > 20 && i < 20);
}

void FanControl::setFanSpeed(int pwm) {
    if (fanControl.is_open()) {
        if (pwm <= 255 && pwm >= 0) {
            fanControl.seekp(0);
            fanControl << pwm << endl;
        } else {
            cerr << "PWM value must be between 0 and 255." << endl;
            throw std::out_of_range("PWM value must be between 0 and 255.");
        }
    } else {
        cerr << "Fancontrol cant set fanspeed, fanRpmPath file not open anymore." << endl;
        throw std::runtime_error("Failed to open fan control file.");
    }
}



void SetFans::declareFanRpmFromTempGraph() {
    getRpm();
    this->fanRpm = getFanRpm();
}

void SetFans::setFanSpeedFromDeclaredRpm() {
    setFanSpeed(this->fanRpm);
}



GetTemperature::~GetTemperature() {
    for (auto &sensor : this->tempSensor) {
        if(sensor.is_open()) {
            sensor.close();
        }
    }
}

FanControl::~FanControl() {

    if (this->fanControl.is_open())
        this->fanControl.close();

    if (this->rpmSensor.is_open())
        this->rpmSensor.close();

    if (this->fanSettingsAutoGenFile.is_open())
        this->fanSettingsAutoGenFile.close();

}