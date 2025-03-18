while true
do
	cat /var/log/syslog | grep ecsysapp
	ps -alx | grep ecsysapp
        sleep 1
done

