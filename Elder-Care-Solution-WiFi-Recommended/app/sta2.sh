sudo nmcli connection add type 802-11-wireless con-name <conn_name> ssid myssid 802-11-wireless-security.key-mgmt WPA-PSK 802-11-wireless-security.psk mypass
sudo nmcli con mod <conn_name> connection.autoconnect true
sudo nmcli connection up <conn_name>
