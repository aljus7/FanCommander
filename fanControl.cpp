#include"fanControl.h"
#include <functional>
#include <regex>
#include <string>
#include <thread>
#include <utility>
using json = nlohmann::json;

GetTemperature::GetTemperature(vector<string> tempPath, vector<vector<pair<int, int>>> tempRpmGraph, string function, int maxPwm, int avgTimes, TempSensorServer* tmpSrv) {
    if (tempPath.size() == tempRpmGraph.size()) {    
        //this->tempSensor.resize(tempPath.size());
        for (int i = 0; i < tempPath.size(); i++) {
            if (tempPath[i] != "null" && !tempPath[i].empty())
                this->tempSensor.emplace_back(ref(tmpSrv->getTempSenseIfstream(tempPath[i])));
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

    this->rpms.resize(tempPath.size());

}

int GetTemperature::averaging(int pwm) {
    int sum = 0;

    // simplification using <deque>
    this->lastPwmValues.push_back(pwm);
    if (this->lastPwmValues.size() > this->avgTimes) {
        this->lastPwmValues.pop_front();
    } 

    // Compute average if we have at least one value
    if (!this->lastPwmValues.empty()) {
        for (int value : this->lastPwmValues) {
            sum += value;
        }
        return sum / this->lastPwmValues.size();
    } else {
        std::cerr << "Averaging error: array is not populated yet." << std::endl;
        return 255;
    }
}

void GetTemperature::getRpm() {
    // Check for size mismatch
    if (this->tempSensor.size() != this->tempRpmGraph.size()) {
        std::cerr << "Size mismatch: tempSensor.size() != tempRpmGraph.size()" << std::endl;
        return;
    }
    vector<int> temps(this->tempSensor.size());
    string tempStr;
    for(int i = 0; i < this->tempSensor.size(); i++) {
        // Check if stream is open
        if (!this->tempSensor[i].get().is_open()) {
            std::cerr << "Temperature sensor stream " << i << " is not open!" << std::endl;
            temps[i] = 0;
            continue;
        }
        this->tempSensor[i].get().seekg(0);
        if (getline(this->tempSensor[i].get(), tempStr)) {
            try {
                temps[i] = std::stod(tempStr)/1000;
            } catch (const std::invalid_argument& e) {
                std::cerr << "Invalid argument: " << e.what() << std::endl;
            } catch (const std::out_of_range& e) {
                std::cerr << "Out of range: " << e.what() << std::endl;
            }
        } else {
            cerr << "Failed to read line " << i << endl;
        }

    }


    for (size_t j = 0; j < temps.size(); ++j) {
        if (j >= tempRpmGraph.size()) {
            std::cerr << "Index " << j << " out of bounds for tempRpmGraph" << std::endl;
            continue;
        }
        const int temp = temps[j];
        const auto& currGraph = tempRpmGraph[j];
        int& rpm = this->rpms[j];

        for (size_t i = 0; i < currGraph.size(); ++i) {
            if (temp <= currGraph[i].first) {
                if (i == 0) {
                    rpm = currGraph[0].second;
                } else {
                    const auto& prev = currGraph[i - 1];
                    const auto& curr = currGraph[i];
                    rpm = prev.second + ((temp - prev.first) * (curr.second - prev.second)) / (curr.first - prev.first);
                }
                break;
            }
        }

        if (temp > currGraph.back().first) {
            rpm = currGraph.back().second;
        }
    }

}

int GetTemperature::getFanRpm() {
    if (this->rpms.empty()) {
        std::cerr << "Error: rpms vector is empty!" << std::endl;
        return 255;
    }
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
        this->fanNamePath = fanPath;
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

    regex nonDigit("[^0-9]+");
    string autoGenFileName = this->stateFilesPath + regex_replace(this->fanNamePath, nonDigit, "") + this->autoGenFileAppend;

    std::ifstream checkFile(autoGenFileName);
    bool fileExists = checkFile.good();
    checkFile.close();

    // Open with trunc if file doesn't exist, else open normally
    std::ios_base::openmode mode = std::ios::in | std::ios::out;
    if (!fileExists) {
    mode |= std::ios::trunc; // Create new file
    }

    fanSettingsAutoGenFile.open(autoGenFileName, mode);

    if (!fanSettingsAutoGenFile.is_open()) {
    std::cerr << "Failed to open file: " << autoGenFileName << std::endl;
    return;
    }

    if (fileExists) {
    getMinStartPwm(fanSettingsAutoGenFile);
    } else {
    writeMinStartPwm(fanSettingsAutoGenFile);
    }

    fanSettingsAutoGenFile.close();

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
            } else if (i == 0) {
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
            int fanRpm = stoi(fanRpmStr);
            if (prevRpm == 256) {    
                prevRpm = fanRpm;
                this_thread::sleep_for(std::chrono::milliseconds(1000));
                diff = abs(prevRpm-fanRpm);
            } else {
                this_thread::sleep_for(std::chrono::milliseconds(1000));
                diff = abs(prevRpm-fanRpm);
                prevRpm = fanRpm;
            }
            ++i;
        }
    } while(diff > 20 && i < 20);
}

void FanControl::getFeedbackRpm() {
    string rpmString;
    int fanRpm;

    if (this->rpmSensor.is_open()) {
        this->rpmSensor.seekg(0);
        if (getline(this->rpmSensor, rpmString)) {
            fanRpm = stod(rpmString);
        } else {
            cerr << "couldnt getline from rpm fan feedback file" << endl;
            fanRpm = 255;
        }
    } else {
        throw std::runtime_error("Failed to open fan rpm feedback file.");
    }

    this->feedBackRpm = fanRpm;
}

void FanControl::setFanSpeed(int pwm) {
    getFeedbackRpm();

    if (fanControl.is_open()) {
        if (pwm <= 255 && pwm >= 0) {
            if (pwm >= this->minPwmGood && this->feedBackRpm == 0) {
                if (255 - this->startPwmGood > 10)  {   
                    fanControl.seekp(0);
                    fanControl << this->startPwmGood + 10 << endl;
                } else {
                    fanControl.seekp(0);
                    fanControl << this->startPwmGood << endl;
                }
            }

            if (pwm >= this->minPwmGood) {    
                fanControl.seekp(0);
                fanControl << pwm << endl;
            } else {
                fanControl.seekp(0);
                fanControl << this->minPwmGood << endl;
            }
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
        if(sensor.get().is_open()) {
            sensor.get().close();
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


TempSensorServer::TempSensorServer(vector<string> tempPaths, vector<string> sensorName) {

    if (tempPaths.size() > 0 && tempPaths.size() == sensorName.size()) {
        for (int i = 0; i < tempPaths.size(); i++) {
            this->tempSensorNamePathCoor.push_back(make_pair(sensorName[i], tempPaths[i]));
        }

        for (int i = 0; i < this->tempSensorNamePathCoor.size(); i++) {
            const string& path = this->tempSensorNamePathCoor[i].second;
            bool match = false;
            if (this->tempSensor.size() == 0) {
                this->tempSensorStreams.emplace_back(path, ios::in);
                if (!this->tempSensorStreams.back().is_open()) {
                    tempSensorStreams.pop_back();
                    throw std::runtime_error("Failed to open file: " + path);
                }
                this->tempSensor.push_back(make_pair(path, ref(tempSensorStreams.back())));
            } else {
                for(int j = 0; j < this->tempSensor.size(); j++) {
                    if(path == tempSensor[j].first) {
                        match = true;
                    } 
                }
                if (!match) {
                    this->tempSensorStreams.emplace_back(path, ios::in);
                    if (!this->tempSensorStreams.back().is_open()) {
                        tempSensorStreams.pop_back();
                        throw std::runtime_error("Failed to open file: " + path);
                    }
                    this->tempSensor.push_back(make_pair(path, ref(this->tempSensorStreams.back())));
                }
            }
        }
    } else {
        throw std::invalid_argument("tempPaths and sensorName must have the same size and should not be empty");
    }

}

ifstream& TempSensorServer::getTempSenseIfstream(const string &sensorPath) {
    for (auto& tempSensPair : this->tempSensor) {
        if (tempSensPair.first == sensorPath) {
            if (!tempSensPair.second.get().is_open()) {
                throw std::runtime_error("Sensor stream for path " + sensorPath + " is not open!");
            }
            return tempSensPair.second.get();
        }
    }
    throw std::runtime_error("Sensor path not found: " + sensorPath);
}