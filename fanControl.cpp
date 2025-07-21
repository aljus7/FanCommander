#include"fanControl.h"

GetTemperature::GetTemperature(vector<string> tempPath, vector<vector<pair<int, int>>> tempRpmGraph, string function, int maxPwm, vector<int> maxTemp, int avgTimes) {
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
            return max;
        }

        else if (this->function == "min" && !this->rpms.empty()) {
            int min = rpms[0];
            for (int i = 1; i < this->rpms.size(); i++) {
                    min = min < rpms[i] ? min : rpms[i];
            }
            return min;
        }

        else if (this->function == "avg" && !this->rpms.empty()) {
            int avg = rpms[0];
            int sum = 0;
            for (int i = 0; i < this->rpms.size(); i++) {
                sum += rpms[i];
            }
            avg = sum/rpms.size();
            return avg;
        }

        else {
            cerr << "function value needs to be either 'max', 'min' or 'avg'" << endl;
            return 255;
        }
        
    } else {
        return this->rpms[0];
    }
}

FanControl::FanControl(string fanPath, string rmpPath, int minPwm, int maxPwm, int startPwm) {

    

}