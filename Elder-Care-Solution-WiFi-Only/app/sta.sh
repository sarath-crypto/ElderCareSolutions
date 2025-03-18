sudo nmcli connection add type 802-11-wireless con-name MyWifi ssid MyWifi 802-11-wireless-security.key-mgmt WPA-PSK 802-11-wireless-security.psk 1234567890
sudo nmcli con mod MyWifi connection.autoconnect true
sudo nmcli connection up MyWifi
