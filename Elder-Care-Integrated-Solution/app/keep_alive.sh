#!/bin/bash
ts=$(date)
echo "$ts ecsysapp initializing..." >> /home/ecsys/app/log.txt
while true
do 
	cmd=$(pgrep ecsysapp)
	if [ -z "$cmd" ]; then
		/home/ecsys/app/ecsysapp
		echo 0 > /sys/class/graphics/fbcon/cursor_blink
		ts=$(date)
		echo "$ts ecsysapp application started" >> /home/ecsys/app/log.txt
	fi
	sleep 60
	if [ ! -f /home/ecsys/app/access.txt ]; then
		ts=$(date)
		echo "$ts ecsysapp bash access file not found rebooting" >> /home/ecsys/app/log.txt
    		sudo reboot
	fi

	ROUTER_IP="192.168.0.1"
        ping -c 1 -W 0.7 $ROUTER_IP > /dev/null 2>&1
        if [ $? -eq 0 ]; then
                ROUTER_STATUS="online"
        else
                ROUTER_STATUS="offline"
        fi

        if [[ $ROUTER_STATUS != "online" ]]; then
		ts=$(date)
                echo "$ts ecsysapp router offline re-setting wlan0" >> /home/ecsys/app/log.txt
                sudo ifconfig wlx9c443d18e94b down
                sudo ifconfig wlx9c443d18e94b up
        fi
done
