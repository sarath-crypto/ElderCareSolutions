while true
do 
	cmd=$(pgrep ecsysapp)
	if [ -z "$cmd" ]; then
		/home/ecsys/app/ecsysapp /home/ecsys/app/config.ini
	fi
	sleep 30
done
