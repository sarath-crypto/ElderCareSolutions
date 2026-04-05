#!/bin/bash
ts=$(date)
echo "$ts ecsysapp initializing..." >> /home/ecsys/app/log.txt
while true
do
	cmd=$(pgrep ecsysapp)
	if [ -z "$cmd" ]; then
		clear
		echo "Waiting for 30 seconds to kickoff the ecsys application"
        	for i in {1..30}; do
			echo -n "."
			sleep 1
		done
		/home/ecsys/app/ecsysapp
		ts=$(date)
		echo "$ts ecsysapp application started" >> /home/ecsys/app/log.txt
                tput civis
                PS1=""
	fi

 

        sleep 40
	rm /home/ecsys/app/access.txt -rf	
	sleep 20
	if [ ! -f /home/ecsys/app/access.txt ]; then
		ts=$(date)
		echo "$ts ecsysapp bash access file not found rebooting" >> /home/ecsys/app/log.txt
		sudo reboot
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
                	echo "$ts ecsysapp router offline rebooting" >> /home/ecsys/app/log.txt
			sudo reboot
                fi
                sleep 1
        done
done
