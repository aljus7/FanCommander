#!/bin/bash

if [ "$(ps -p 1 -o comm=)" = "systemd" ]; then
    echo "[ OK ] Systemd is present."
else
    echo "System does not use systemd. Eixting ..."
    exit -1
fi

configExample = '{
    "settings": {
        "refreshInterval": 1000
    },
    
    "tempSensors": [
        {
            "sensor": "CPU",
            "path": "/sys/class/hwmon/hwmon3/temp1_input",
            "graph": [
                {"temp": 48, "pwm": 85},
                {"temp": 50, "pwm": 100},
                {"temp": 60, "pwm": 130},
                {"temp": 70, "pwm": 255}
            ]
        },
        {
            "sensor": "GPU",
            "path": "/sys/class/hwmon/hwmon6/temp1_input",
            "graph": [
                {"temp": 48, "pwm": 85},
                {"temp": 50, "pwm": 100},
                {"temp": 60, "pwm": 180},
                {"temp": 70, "pwm": 255}
            ]
        }
    ],

    "fans": [
        {
            "fanControlPath": "/sys/class/hwmon/hwmon4/pwm4",
            "fanRpmPath": "/sys/class/hwmon/hwmon4/fan4_input",
            "sensors": ["GPU", "CPU"],
            "sensorFunction": "max",
            "averageSampleSize" : 10,
            "minPwm": 60,
            "startPwm": 70,
            "maxPwm": 255
        },
        {
            "fanControlPath": "/sys/class/hwmon/hwmon4/pwm1",
            "fanRpmPath": "/sys/class/hwmon/hwmon4/fan1_input",
            "sensors": ["GPU", "CPU"],
            "sensorFunction": "max",
            "averageSampleSize" : 10,
            "minPwm": 60,
            "startPwm": 70,
            "maxPwm": 255
        }
    ]

}'

systemdUnit = '[Unit]
Description=Custom Fan Control Daemon, fanCommander
After=multi-user.target
  
[Service]
ExecStartPre=/bin/sleep 6
ExecStart=/usr/local/bin/fanCommander
Restart=always
User=root
Nice=14
  
[Install]
WantedBy=multi-user.target'

preSleepUnit = '[Unit]
Description=Stop FanCommander before suspend
Before=sleep.target

[Service]
Type=oneshot
ExecStart=/bin/systemctl stop fanCommander.service

[Install]
WantedBy=sleep.target'

resumeUnit = '[Unit]
Description=Restart FanCommander after resume
After=suspend.target

[Service]
Type=oneshot
ExecStart=/bin/systemctl restart fanCommander.service

[Install]
WantedBy=suspend.target'

echo '\nIs this a fresh install of fancontrol tool (Do you want to run "sensors-detect" ?).'
echo -n "(y/N): "
read -n 1 input
echo

if ["$input" = "y"]
    sensors-detect || echo "[ FAIL ] Did you install lm-sensors?" && exit -1
else
    echo "Skipping 'sensors-detect'!"
fi

cd pwd

echo "Next commands will require root password. Install is system wide."

sudo cp ./fanCommander /usr/local/bin/ || echo "[ FAIL ] Does fanCommander binary exist in same folder as script is run from?" && exit -1

sudo echo $configExample > /etc/fanCommander/config.json

sudo echo $systemdUnit > /etc/systemd/system/fanCommander.service

sudo echo $preSleepUnit > /etc/systemd/system/fancommander-pre-sleep.service

sudo echo $resumeUnit > /etc/systemd/system/fancommander-resume.service

echo "After you configure '/etc/fanCommander/config.json', run thease commands in terminal:\n
sudo systemctl enable fanCommander.service\n
sudo systemctl enable fancommander-pre-sleep.service\n
sudo systemctl enable fancommander-resume.service\n
\n
To start fancommander now run:\n
sudo systemctl start fanCommander.service\n
or for easier debuging run in terminal:\n
fanCommander" 
