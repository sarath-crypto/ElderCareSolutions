while true
do
	tail -1000 /var/log/syslog | grep solapp
	ps -alx | grep solapp
        sleep 2
done

