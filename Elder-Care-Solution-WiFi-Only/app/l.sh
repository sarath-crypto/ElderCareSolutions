while true
do
	tail -500 /var/log/syslog | grep ecsysapp
	ps -alx | grep ecsysapp
        sleep 1
done

