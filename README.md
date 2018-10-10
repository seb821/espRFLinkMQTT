# espRFLinkMQTT
ESP8266 gateway between RFLink and MQTT server

This code allows to use an ESP8266 (ESP01, D1 mini, ...) to forward data from an [RFLink](http://rflink.nl) board to an MQTT server.

Code is inspired from [rflink-to-mqtt](https://github.com/Phileep/rflink-to-mqtt), [RflinkToJsonMqtt](https://github.com/jit06/RflinkToJsonMqtt) and [RFLink-to-FHEM-via-MQTT](https://github.com/lubeda/RFLink-to-FHEM-via-MQTT/)

The following capabilities were added:
- Possbility to publish only a list of user ID to MQTT server
- MQTT username, password and will topic
- Adds a new unique field if a CMD field follows a SWITCH field (ex: "SWITCH02" = "ON" added to "SWITCH"="02", CMD="ON")
- Hardware reset MEGA (RFLink) with ESP pin
- HTTP server to show data received and how it is converted, with javascript functions to filter
- Debug mode to publish on MQTT server with possibility to enable/disable
- Uptime published on MQTT server every 5 minutes
- OTA plus firmware update from webserver
- Send commands from web interface with a predefined user list
- Change easily serial interface to be used : hardware serial RX/TX, software serial on user defined pins, or a mix ; by default it listens to RFLink on RX and writes to software serial on GPIO2/D4
- Handles negative values for fields TMP, WINCHL, WINTMP (not tested in real conditions so far)
- Check data received is ASCII
- Only publish on data change but still publish after a user defined period (a zero value means publish all the time)
- Show setup information on web interface plus last received / published information for each user ID
- Use of [ESP Easy](https://github.com/letscontrolit/ESPEasy) CSS for nice web interface

## Setup:

- RFLink-Hardware required (official or DIY)
- Edit options in espRFLinkMQTT.ino : authorized IDS to forward to MQTT server, commands to show on web interface and serial configuration to use (hardware, software or a mix)
- Edit options in Common.h for Wi-Fi and MQTT settings
- Compile and upload
- Wire serial as defined in serial configuration ; by default, wire ESP RX pin  to RFLink TX pin, and ESP GPIO2/D4 pin to RFLINK RX pin
- For hardware reset of MEGA (RFLink) from ESP, wire ESP pin defined by MEGA_RESET_PIN to MEGA RST pin ; by default, is uses GPIO/D3 pin
- Subscribe to MQTT topic 'rflink/#'
- Use web interface by pointing your http browser to ESP IP

## How to use

When RFLink receives something this is presented on the serial line eg.:

```
20;2A;Xiron;ID=2801;TEMP=0043;HUM=29;BAT=OK;
```

This software published this to the topics:

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
