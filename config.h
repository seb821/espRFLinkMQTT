#ifndef H_CONFIG
#define H_CONFIG


//********************************************************************************
// Serial and hardware configuration
//********************************************************************************

//#define ENABLE_SERIAL_DEBUG					// uncomment to enable debug on debugSerialTX

#ifdef ENABLE_SERIAL_DEBUG
	HardwareSerial & debugSerialTX = Serial;	// debugSerialTX is to show information for debugging - use Serial to write on hardware serial (ESP TX pin) 
#endif


//********************************************************************************
// Wi-Fi parameters
//********************************************************************************

#define WIFI_SSID ""						// WiFi network SSID for ESP8266 to connect to
#define WIFI_PASSWORD ""					// WiFi password for the network above
#define HOSTNAME "espRFLinkMQTT" 			// Client name used for hostname, access point name, OTA, client ID for MQTT server

#define ENABLE_WIFI_SETTINGS_ONLINE_CHANGE
/*
If not defined (line commented): ESP tries to connect to WiFi using SSID and password provided above. These are included in the firmware and cannot be changed later on unless flashing with a new firmware. 

If defined (line uncommented):
- WiFi SSID and password can be changed from the web interface in the 'System' tab (page at url /infos).
- In addition, the startup sequence is changed to allow access in case wrong WiFi credentials are provided:
	- At startup, ESP tries to connect to WiFi using SSID and password saved in EEPROM memory (persistent memory).
	- If it succeeds, espRFLinkMQTT finishes its startup.
	- If it fails to connect, after a period of time (WIFI_CONNECT_TIMEOUT, 60 seconds by default), it starts a WiFi Access Point.
	- The access point name is HOSTNAME defined above (espRFLinkMQTT by default).
	- By connecting to this access point, it is possible to change WiFi settings by pointing browser to url http://192.168.4.1/infos
	- Once wifi settings are applied, just reboot the ESP by clicking on "Reboot ESP".
	- The access point stays only available for a duration of WIFI_AP_TIMEOUT (5 minutes by default). After that time, ESP goes back to a normal connection mode and tries again to connect using WiFi credentials in memory. The goal is to allow ESP to connect in case it started before WiFi router and thus WiFi was not immediately available. Otherwise, ESP would stay blocked in access point mode.
*/

#define WIFI_CONNECT_TIMEOUT		60 * 1000			// WiFi duration ESP tries to connect to WiFi at boot (60 seconds by default)
#define WIFI_AP_TIMEOUT				5 * 60 * 1000		// Access Point timeout (5 minutes by default)

//********************************************************************************
// MQTT parameters
//********************************************************************************

#define ENABLE_MQTT_SETTINGS_ONLINE_CHANGE		// If defined, the four settings below can be changed in the webinterface

#define MQTT_SERVER "192.168.1.1"					// MQTT Server
#define MQTT_PORT 1883								// MQTT server port
#define MQTT_USER ""								// MQTT Server user
#define MQTT_PASSWORD ""							// MQTT Server password   
#define MQTT_PUBLISH_TOPIC "rflink"					// MQTT topic to publish to (data from RFLink to MQTT)
#define MQTT_RFLINK_CMD_TOPIC "rflink/cmd"			// MQTT topic to listen to (commands from MQTT to RFLink)
#define MQTT_RETAIN_FLAG false						// If true, messages will be published with the retain flag
#define MQTT_WILL_TOPIC "rflink/online"				// MQTT last will topic ; "rflink/status"
#define MQTT_WILL_ONLINE "1"						// MQTT last will topic value online; "online"
#define MQTT_WILL_OFFLINE "0"						// MQTT last will topic value offline; "offline"

#define MQTT_DEBUG_TOPIC "rflink/debug"				// MQTT debug topic to publish raw data, name, ID, MQTT topic and daata json. This is published using a json format.
#define MQTT_UPTIME_TOPIC "rflink/uptime"			// MQTT topic for uptime
#define MQTT_RSSI_TOPIC "rflink/rssi"				// MQTT topic for WiFi RSSI


#define MQTT_MEGA_RESET_TOPIC "rflink/mega_reset"	// MQTT topic whereto publish a 1s pulse when resetting RFLink Mega


#define CONFIG_VERSION 20200514
// Changing this number overwrites the configuration in eeprom memory with configuration in this file (config.h).
// =====>>> In general to be safe, you should always update this number after making changes in this file <<<===== 

/*
This is how CONFIG_VERSION works :
- At startup software reads config version in eeprom memory.
- If value is the same as above, it uses eeprom memory configuration.
- If it is different, it overwrites eeprom memory configuration with configuration contained in this file (config.h)
This applies to WiFi credentials, MQTT settings, ID filtering configuration.
It means that if changes were made online, it is safer to update configuration in this file not to lose them on next firmware update. 
*/


#endif