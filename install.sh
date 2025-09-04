#!/bin/bash

# Check if system uses systemd
ps -p 1 -o comm= | grep -q "systemd" || { echo "System does not use systemd. Exiting..."; exit 1; }

echo "[ OK ] Systemd is present."

# JSON configuration
configExample='{
    "settings": {
        "refreshInterval": 500
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
                {"temp": 48, "pwm": 90},
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
            "maxPwm": 255,
            "overrideMax": false,
            "proportionalFactor" : 0.05
        },
        {
            "fanControlPath": "/sys/class/hwmon/hwmon4/pwm1",
            "fanRpmPath": "/sys/class/hwmon/hwmon4/fan1_input",
            "sensors": ["GPU", "CPU"],
            "sensorFunction": "max",
            "averageSampleSize" : 10,
            "minPwm": 60,
            "startPwm": 70,
            "maxPwm": 255,
            "overrideMax": false,
            "proportionalFactor" : 0.05
        }
    ]

}'

# systemd service units
systemdUnit='[Unit]
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

preSleepUnit='[Unit]
Description=Stop FanCommander before suspend
Before=sleep.target

[Service]
Type=oneshot
ExecStart=/bin/systemctl stop fanCommander.service

[Install]
WantedBy=sleep.target'

resumeUnit='[Unit]
Description=Restart FanCommander after resume
After=suspend.target

[Service]
Type=oneshot
ExecStart=/bin/systemctl restart fanCommander.service

[Install]
WantedBy=suspend.target'

# Prompt for running sensors-detect
echo -e "\nIs this a fresh install of fancontrol tool (Do you want to run 'sensors-detect')?"
echo -n "(y/N): "
read -n 1 input
echo

if [ "${input,,}" = "y" ]; then
    command -v sensors-detect >/dev/null 2>&1 || { echo "[ FAIL ] lm-sensors is not installed. Install it (e.g., 'sudo apt install lm-sensors')"; exit 1; }
    sudo sensors-detect --auto || { echo "[ FAIL ] sensors-detect failed"; exit 1; }
else
    echo "Skipping 'sensors-detect'!"
fi

echo "Next commands will require root password. Install is system-wide."

# Copy fanCommander binary
[ -f "./fanCommander" ] || { echo "[ FAIL ] fanCommander binary does not exist in the current directory"; exit 1; }
sudo cp ./fanCommander /usr/local/bin/ || { echo "[ FAIL ] Failed to copy fanCommander to /usr/local/bin"; exit 1; }
echo "[ OK ] Copied fanCommander to /usr/local/bin"

# Create configuration directory and write config.json
sudo mkdir -p /etc/fanCommander || { echo "[ FAIL ] Failed to create /etc/fanCommander"; exit 1; }
echo "$configExample" | sudo tee /etc/fanCommander/config.json >/dev/null || { echo "[ FAIL ] Failed to write /etc/fanCommander/config.json"; exit 1; }
echo "[ OK ] Wrote configuration to /etc/fanCommander/config.json"

# Write systemd service units
echo "$systemdUnit" | sudo tee /etc/systemd/system/fanCommander.service >/dev/null || { echo "[ FAIL ] Failed to write fanCommander.service"; exit 1; }
echo "[ OK ] Wrote fanCommander.service"

echo "$preSleepUnit" | sudo tee /etc/systemd/system/fancommander-pre-sleep.service >/dev/null || { echo "[ FAIL ] Failed to write fancommander-pre-sleep.service"; exit 1; }
echo "[ OK ] Wrote fancommander-pre-sleep.service"

echo "$resumeUnit" | sudo tee /etc/systemd/system/fancommander-resume.service >/dev/null || { echo "[ FAIL ] Failed to write fancommander-resume.service"; exit 1; }
echo "[ OK ] Wrote fancommander-resume.service"

# Extract and write modules to /etc/modules-load.d/modules.conf
[ -f /etc/conf.d/lm_sensors ] || { echo "[ FAIL ] /etc/conf.d/lm_sensors not found. Run 'sensors-detect' first."; exit 1; }
modules=$(grep -E '^(HWMON_MODULES|BUS_MODULES)="[^"]+"' /etc/conf.d/lm_sensors | awk -F'"' '{print $2}' | tr ' ' '\n') || { echo "[ FAIL ] Failed to extract modules from /etc/conf.d/lm_sensors"; exit 1; }
[ -z "$modules" ] || { echo "$modules" | sudo tee /etc/modules-load.d/modules.conf >/dev/null || { echo "[ FAIL ] Failed to write /etc/modules-load.d/modules.conf"; exit 1; }; }
echo "[ OK ] Wrote modules to /etc/modules-load.d/modules.conf"

# Reload systemd daemon to recognize new units
sudo systemctl daemon-reload || { echo "[ FAIL ] Failed to reload systemd daemon"; exit 1; }
echo "[ OK ] Reloaded systemd daemon"
echo "DONT FORGET TO RESTART COMPUTER"

# Instructions for the user
echo -e "\nSetup complete! To enable and start the services, run the following commands:"
echo "  sudo systemctl enable fanCommander.service"
echo "  sudo systemctl enable fancommander-pre-sleep.service"
echo "  sudo systemctl enable fancommander-resume.service"
echo -e "\nTo start fanCommander now, run:"
echo "  sudo systemctl start fanCommander.service"
echo -e "\nFor debugging, run directly in the terminal:"
echo "  fanCommander"
echo "DONT FORGET TO RESTART COMPUTER before configuring fanCommander"