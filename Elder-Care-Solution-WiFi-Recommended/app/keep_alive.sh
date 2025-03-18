#!/bin/bash
while true
do 
	cmd=$(pgrep ecsysapp)
	if [ -z "$cmd" ]; then
		/home/ecsys/app/ecsysapp
	fi
	sleep 30
	if [ ! -f /home/ecsys/app/access.txt ]; then
		logger ecsysapp bash access file not found rebooting
    		sudo reboot
	fi
done
