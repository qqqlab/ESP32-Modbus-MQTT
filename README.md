## ESP32-Modbus-MQTT

Queries a EHE-N3KTL Solar Inverter and posts the measurements to an MQTT server

Needs an ESP32 board plus a RS-485 / MAX485 board. Connect the GND, +5V, TX, RX, REDE pins to the ESP32, and connect GND, A, B pins to the Modbus connector.

Upon initial startup the ESP32 is in captive portal mode, connect the WiFi network "qqqlab-XXXX" with password "12345678". Then enter your WiFi network name and password. The ESP32 will reboot and connect to your network. 

Open a browser and connect to the IP of the ESP32, and setup the MQTT properties

On your server, use the mqtt2db.php script to log the MQTT data to a database. Add this to /etc/rc.local to start the script on boot:
```
#!/bin/bash

/bin/php /path/to/mqtt2db.php >>/path/to/mqtt2db.log &

exit 0
```