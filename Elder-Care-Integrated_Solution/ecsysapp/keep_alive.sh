#!/bin/bash

cam=$(lsusb | grep "Lenovo Lenovo FHD Webcam Audio")
if [[ -z "$cam" ]]; then
	ts=$(date)
	echo "$ts Usb camera not found rebooting in 30 seconds" >> /var/www/html/log.txt
	sleep 30
	echo "ecsys123" | sudo -S reboot
fi

ts=$(date)
echo "$ts Solution initializing in 60 seconds..." >> /var/www/html/log.txt
sleep 60

while true
do
	if [ -f /home/ecsys/ecsysapp/hwrst.txt ]; then
		rm /home/ecsys/ecsysapp/hwrst.txt -rf	
		ts=$(date)
		echo "$ts hardware error rebooting" >> /var/www/html/log.txt
		echo "ecsys123" | sudo -S reboot
	fi

	pid=$(pgrep ecsysapp)
	if [[ -z "$pid" ]]; then
		pid=$(pgrep solapp)
		if [[ ! -z "$pid" ]]; then
			echo "ecsys123" | sudo -S kill -9 $pid
		fi

		ts=$(date)
		echo "$ts starting application ecsysapp" >> /var/www/html/log.txt
		/home/ecsys/ecsysapp/ecsysapp
		sleep 5
		tput civis
		PS1=""
        fi

	pid=$(pgrep solapp)
	if [[ -z "$pid" ]]; then
		ts=$(date)
		echo "$ts starting application solapp" >> /var/www/html/log.txt
		/home/ecsys/solapp/solapp
        fi


        for i in {1..60}; do
                ROUTER_IP="192.168.0.1"
                ping -c 1 -W 0.7 $ROUTER_IP > /dev/null 2>&1
                if [ $? -eq 0 ]; then
                        ROUTER_STATUS="online"
                else
                        ROUTER_STATUS="offline"
                fi

                if [[ $ROUTER_STATUS == "online" ]]; then
                        break
                fi
                if [ $i -eq 60 ]; then
			ts=$(date)
                	echo "$ts ecsysapp router offline trying to bring up wifi" >> /var/www/html/log.txt
			echo "ecsys123" | sudo -S nmcli con up sarath_nivas_EXT
			sleep 60
			wl=$(cat /sys/class/net/wlan0/operstate)
                	if [[ $wl == "down" ]]; then
				ts=$(date)
                		echo "$ts ecsysapp router offline rebooting" >> /var/www/html/log.txt
				echo "ecsys123" | sudo -S reboot
			fi
                fi
                sleep 1
        done
	sleep 1
done
