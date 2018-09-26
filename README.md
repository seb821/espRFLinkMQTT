# espRFLinkMQTT
ESP8266 gateway between RFLink and MQTT server

This code allows to use an ESP8266 (ESP01, D1 mini, ...) to forward data from an [RFLink](http://rflink.nl) board to an MQTT server.

It is based on code from [RflinkToJsonMqtt](https://github.com/jit06/RflinkToJsonMqtt) and [RFLink-to-FHEM-via-MQTT](https://github.com/lubeda/RFLink-to-FHEM-via-MQTT/) with the following improvements :


## Setup:

- RFLink-Hardware
- Edit options in espRFLinkMQTT.ino : authorized IDS to forward to MQTT server, commands to show on web interface and serial configuration to use (hardware, software or a mix)
- Edit options in Common.h for Wi-Fi and MQTT settings
- Compile and upload
- Wire as defined in serial configuration
- Subscribe to MQTT topic 'rflink/#'
- Use web interface by pointing your http browser to ESP IP

## How to use

When RFLink receives something this is presented on the serial line eg.:

```
20;2A;Xiron;ID=2801;TEMP=0043;HUM=29;BAT=OK;
```

This software published this to the topics:

```
RFLink/Xiron-2801
```
as json like this:
```
{TEMP:"6.7",HUM:"29",BAT:"OK"};
```

## How to send commands
### normal

publish the command according to the [documentation](http://www.rflink.nl/blog2/protref) to the topic

```
rflink/cmd
```

e.g.
```
10;NewKaku;01dd77d5;1;OFF;
```

# Watchout
Use it at your own risk!!!
