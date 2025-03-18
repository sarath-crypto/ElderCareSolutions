sudo nmcli connection add type wifi ifname wlan0 con-name ecsys_ap autoconnect yes ssid ecsys_ap 
sudo nmcli connection modify ecsys_ap wifi-sec.key-mgmt wpa-psk
sudo nmcli connection modify ecsys_ap wifi-sec.psk 12345678
sudo nmcli connection modify ecsys_ap ipv4.addresses 10.10.10.1/24
sudo nmcli connection modify ecsys_ap connection.autoconnect yes
sudo nmcli connection modify ecsys_ap 802-11-wireless.mode ap 802-11-wireless.band bg ipv4.method shared
sudo nmcli connection up ecsys_ap
