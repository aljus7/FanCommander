![Project logo](https://github.com/aljus7/FanCommander/blob/main/logo.png)
# Install:
### Dependencies:
lm-sensors
### sensors-detect:
Run ```sensors-detect``` in terminal.
This will detect all of the sensors present and they will be used for fancontrol. After that, run the following to check if it detected the sensors correctly:

lm-sensors
Warning
The following command is safe by default (pressing Enter at each prompt). Some advanced options may damage hardware: only modify the defaults if you understand the implications.
The first thing to do is to run****
``` bash
$ sensors
coretemp-isa-0000
Adapter: ISA adapter
Core 0:      +29.0°C  (high = +76.0°C, crit = +100.0°C)
...
it8718-isa-0290
Adapter: ISA adapter
Vcc:         +1.14 V  (min =  +0.00 V, max =  +4.08 V)
VTT:         +2.08 V  (min =  +0.00 V, max =  +4.08 V)
+3.3V:       +3.33 V  (min =  +0.00 V, max =  +4.08 V)
NB Vcore:    +0.03 V  (min =  +0.00 V, max =  +4.08 V)
VDRAM:       +2.13 V  (min =  +0.00 V, max =  +4.08 V)
fan1:        690 RPM  (min =   10 RPM)
temp1:       +37.5°C  (low  = +129.5°C, high = +129.5°C)  sensor = thermistor
temp2:       +25.0°C  (low  = +127.0°C, high = +127.0°C)  sensor = thermal diode
```
Note:
If the output does not display an RPM value for the CPU fan, one may need to #Increase the fan divisor for sensors. If the fan speed is shown and higher than 0, this is fine.

*** Source: ArchWiki; https://wiki.archlinux.org/title/Fan_speed_control; 25.6.2025 ***

### Must do:
The enumerated hwmon symlinks located in /sys/class/hwmon/ might vary in order because the kernel modules do not load in a consistent order per boot. Because of this, it may cause FanCommander to not function correctly.

Solution
In /etc/conf.d/lm_sensors, there are 2 arrays that list all of the modules detected when you execute sensors-detect. These get loaded in by fancontrol. If the file does not exist, run sensors-detect as root, accepting the defaults. Open (or create) /etc/modules-load.d/modules.conf. Get all of the modules listed from the 2 variables in /etc/conf.d/lm_sensors and place them into the /etc/modules-load.d/modules.conf file, one module per line. Specifying them like this should make a defined order for the modules to load in, which should make the hwmon paths stay where they are and not change orders for every boot. If this does not work, I highly recommend finding another program to control your fans. If you cannot find any, then you could try using the alternative solution below.

*** Source: ArchWiki; https://wiki.archlinux.org/title/Fan_speed_control; 25.6.2025 ***

### Install:
You can try to run: https://github.com/aljus7/FanCommander/blob/main/install.sh to go through wizard.<br>
Dont forget to ```chmod 744 ./install.sh```<br><br>
If wizard install doesent work for you, you either can fix it or follow instructions:<br>
1. Copy fancontrol binary to /usr/local/bin (recommended) or any other location: <br>
```sudo cp ./fanCommander /usr/local/bin/``` <br>
2. Make executable: <br>
```sudo chmod 755 /usr/local/bin/fanCommander``` <br>
3. Make system.d service to run it at start: <br>
    - ```sudo touch /etc/systemd/system/fanCommander.service``` <br>
    - Example systemd service:
      ``` bash
      [Unit]
      Description=Custom Fan Control Daemon, fanCommander
      After=multi-user.target
        
      [Service]
      ExecStartPre=/bin/sleep 6
      ExecStart=/usr/local/bin/fanCommander
      Restart=always
      User=root
      Nice=14
        
      [Install]
      WantedBy=multi-user.target
      ```
   - Enable and start that service.
   - If you want for sleep to work you also need:
     ```
     [Unit]
     Description=Stop FanCommander before suspend
     Before=sleep.target
        
     [Service]
     Type=oneshot
     ExecStart=/bin/systemctl stop fanCommander.service
        
     [Install]
     WantedBy=sleep.target
     ```
     and
     ```
     [Unit]
     Description=Restart FanCommander after resume
     After=suspend.target
        
     [Service]
     Type=oneshot
     ExecStart=/bin/systemctl restart fanCommander.service
        
     [Install]
     WantedBy=suspend.target
     ```
    - Enable thease two services
    - all of those services are usually placed in '/etc/systemd/system/'
    - Good luck.
# Configure:
Config file needs to be located under: "/etc/fanCommander/config.json".
overrideMax - overrides user set maxPwm value with internally calculated one. That feature is set to false if proportionalFactor is more than 0.
proportionalFactor - 0 is OFF, > 0 is ON. Set proportioanl value for proportional fan error adjustment.
Example config file:
``` json
{
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

}
```
