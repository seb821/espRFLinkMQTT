# espRFLinkMQTT
ESP8266 gateway between RFLink and MQTT server

This code allows to use an ESP8266 (ESP01, D1 mini, ...) to forward data from an [RFLink](http://rflink.nl) board to an MQTT server with messages in JSON.

Code is inspired from [rflink-to-mqtt](https://github.com/Phileep/rflink-to-mqtt), [RflinkToJsonMqtt](https://github.com/jit06/RflinkToJsonMqtt) and [RFLink-to-FHEM-via-MQTT](https://github.com/lubeda/RFLink-to-FHEM-via-MQTT/)

The following capabilities were added:
- MQTT username, password and will topic
- Adds a new unique field if a CMD field follows a SWITCH field (ex: "SWITCH02" = "ON" added to "SWITCH"="02", CMD="ON")
- Hardware reset RFLink MEGA with ESP pin
- Auto reset RFLink MEGA if no data is received within a specific time frame (disabled by default, set resetMegaInterval in ms to enable) 
- HTTP server to show data received and how it is converted, with javascript functions to filter or sort colum by clicking on headers (Live data tab only)
- Debug mode to publish on MQTT server with possibility to enable/disable
- Uptime published on MQTT server every 5 minutes
- OTA firmware update from webserver
- Send commands from web interface with possibility to setup a predefined user list
- Change easily serial interface to be used : hardware serial RX/TX, software serial on user defined pins, or a mix ; by default it listens to RFLink MEGA on ESP RX pin and writes to RFLink MEGA with software serial on ESP GPIO2/D4
- Handles negative values for fields TMP, WINCHL, WINTMP
- Check data received is ASCII
- Possibility to publish only a list of user IDs to MQTT server. In addition, possibility to publish only on data change but still to publish after a user defined time frame (disabled by default - change USER_ID_NUMBER and USER_IDs to enable)
- Show setup information on web interface plus last received / published time for each defined USER ID
- Use of [ESP Easy](https://github.com/letscontrolit/ESPEasy) CSS for nice web interface

## Setup:

- RFLink-Hardware required (official or DIY)
- Edit options in Common.h for Wi-Fi and MQTT settings
- Edit options in espRFLinkMQTT.ino (optional) :
	- Update USER_IDs table with your own devices if you want to use ID filtering 
	- Change USER_ID_NUMBER with the number of USER_IDs table lines
 authorized IDS to forward to MQTT server, commands to show on web 
	 - Modify USER_CMDs table to show you own commands on the web interface
	 - Change serial configuration if you want want to use other pins
- Compile with Arduino IDE - You will need the following libraries:
	- ArduinoJson library 5.13.x, not version 6.x.x (Sketch > Include Library > Manage Librairies > ArduinoJson > Select version 5.13.x > Install)
	- PubSubClient
- Upload to ESP
- Wire serial as defined in serial configuration ; **wire ESP RX pin to RFLink MEGA TX pin, and ESP GPIO2/D4 pin to RFLINK MEGA RX pin** (this is default, see not below to change it)
- To handle hardware reset of RFLink MEGA from ESP, wire ESP GPIO/D3 pin to RFLink MEGA RST (this is default ; it can be changed with MEGA_RESET_PIN in Common.h)
- Subscribe to MQTT topic 'rflink/#'
- Use web interface by pointing your http browser to ESP IP

**Note**:  If you want to use **ESP RX/TX** connected to RFLink MEGA TX/RX, you need to change this:
	- auto& debugSerialTX = Serial; ⇒ auto& debugSerialTX = softSerial;
	- auto& rflinkSerialTX = softSerial; ⇒ auto& rflinkSerialTX = Serial;

## How to use

When RFLink receives something this is presented on the serial line eg.:

```
20;2A;Xiron;ID=2801;TEMP=0043;HUM=29;BAT=OK;
```

This software publishes this to the topics:

```
rflink/Xiron-2801
```
as json like this:
```
{TEMP:"6.7",HUM:"29",BAT:"OK"};
```

## How to send commands

### MQTT

Publish the command according to the [documentation](http://www.rflink.nl/blog2/protref) to the topic

```
rflink/cmd
```

e.g.
```
10;NewKaku;01dd77d5;1;OFF;
```
### Web interface

On the web interface main page, use the form  or a predefined command button

![alt tag](https://i.imgur.com/Xl1tLUz.png "espRFLink")

# Watchout

Use it at your own risk!!!
