#ifndef H_CONFIG
#define H_CONFIG


//********************************************************************************
// Serial and hardware configuration
//********************************************************************************

//#define ENABLE_SERIAL_DEBUG					// uncomment to enable debug on debugSerialTX

#ifdef ENABLE_SERIAL_DEBUG
	HardwareSerial & debugSerialTX = Serial;	// debugSerialTX is to show information for debugging - use Serial to write on hardware serial (ESP TX pin) 
#endif
HardwareSerial & rflinkSerialRX = Serial;		// rflinkSerialRX is used for data from RFLink - uncomment this line to listen on hardware serial (ESP RX pin)
HardwareSerial & rflinkSerialTX = Serial;		// rflinkSerialTX is used for commands to RFLink
//#include <SoftwareSerial.h>					// rflinkSerialTX is used for commands to RFLink: by default, it uses ESP TX pin ; in order to use another pin, comment previous line and uncomment this line and two following ones to write on software serial (GPIO2/D4) - software serial RX listens on GPIO4/D2 pin (unused)
//SoftwareSerial softSerial(4, 2, false);		// software serial TX to RFLink on GPIO2/D4 ; software serial RX from GPIO4/D2 pin (unused) 
//SoftwareSerial & rflinkSerialTX = softSerial;          

#define DEFAULT_MEGA_RESET_PIN -1				// ESP pin connected to MEGA reset pin ; this is the default value and it can be changed from the web interface ; examples: -1 for none, 16 for GPIO16 (D0)
#define DEFAULT_MEGA_AUTO_RESET_INTERVAL 0 * 60 * 1000	// Auto reset MEGA if no data is received during this period of time (in ms), 0 to disable ; this is the default value and it can be changed from the web interface

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


//********************************************************************************
// ID filtering
//********************************************************************************
/*
In order to avoid polluting MQTT server, it is possible with ID filtering to publish on MQTT server only information provided by some specific ID. This can be enabled or disabled online.
*/

#define ID_FILTERING	false

const _filtered_IDs filtered_IDs[] = {
/*
This table allows to configure default values for IDs that will be filtered on before being sent to MQTT server.
- First column is ID that will be filtered for - max 10 characters.
- Second column is ID applied for MQTT topic - max 10 characters.
- Third column is interval time (in ms) to force publication: if data received has not changed within this interval time, it will not be published (note that if data received changed it is always published). Use 0 to publish everytime some data is received
- Last column is a description - max 24 characters.
- This configuration can be modified online.
- Default number of devices is 32.
- Leave an empty ID after last device line (avoids comparing with non relevant lines).
*/

// Default ID filtering configuration with 9 devices which ID are filtered for
		{"1082","1082",30 * 60 * 1000,"Auriol V3"},					// 1
		{"0210","0210",30 * 60 * 1000,"Alecto V5"},					// 2
		{"2A04","2A1C",30 * 60 * 1000,"Oregon Rain2"},				// 3
		{"00004","00000",0,"device always updated"},				// 4
		{"00005","00000",30 * 60 * 1000,"device max 30min update"},	// 5
		{"00006","00000",1 * 1000,"device max 1s update"},			// 6
		{"00007","00000",1 * 1000,"device #7 description"},			// 7
		{"00008","00000",1 * 1000,"device #8 description"},			// 8
		{"00009","00000",1 * 1000,"last device description"},		// 9
		{"","",0,"no ID after last device"},						// 10
		{"","",0,"#11"},											// 11
		{"","",0,"#12"},											// 12
		{"","",0,"#13"},											// 13
		{"","",0,"#14"},											// 14
		{"","",0,"#15"},											// 15
		{"","",0,"#16"},											// 16
		{"","",0,"#17"},											// 17
		{"","",0,"#18"},											// 18
		{"","",0,"#19"},											// 19
		{"","",0,"#20"},											// 20
		{"","",0,"#21"},											// 21
		{"","",0,"#22"},											// 22
		{"","",0,"#23"},											// 23
		{"","",0,"#24"},											// 24
		{"","",0,"#25"},											// 25
		{"","",0,"#26"},											// 26
		{"","",0,"#27"},											// 27
		{"","",0,"#28"},											// 28
		{"","",0,"#29"},											// 29
		{"","",0,"#30"},											// 30
		{"","",0,"#31"},											// 31
		{"","",0,"#32"},											// 32
};
// Note: ID filtering configuration is now saved in eeprom memory and can be changed online in the /configuration page. This is very useful when a device changes ID in order to keep using the same MQTT topic. If changes are made online, the /configuration page provides directly the new code to update here.

#define CONFIG_VERSION 20200430
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

const char* const user_specific_ids[][3] = {    // This is used to force a specific ID for devices changing frequently ; it applies to a specific protocol to all IDs (put ALL) or to a specific ID
  //{ "Auriol_V3"  ,  "ALL"  , "1082"   },
  //{ "Alecto_V5"  ,  "0001" , "0210"   },
};


//********************************************************************************
// Interface parameters
//********************************************************************************

const char* const user_cmds[][2] = {			// These are predefined user commands to RFLink appearing in the web interface
  { "PING"				,  "10;ping;"								},
  { "VERSION"			,  "10;version;"							},
  { "STATUS"			,  "10;status;"								},
  { "REBOOT"			,  "10;reboot;"								},
};


//********************************************************************************
// RFLink Wifi board
// https://www.nodo-shop.nl/en/home/191-rflink-wifi-koppelprint.html
//********************************************************************************

// Uncomment following line if using this board in order to use watchdog functions
#define RFLINK_WIFI_BOARD

#ifdef RFLINK_WIFI_BOARD
  #define DEFAULT_PIN_I2C_SDA              4
  #define DEFAULT_PIN_I2C_SCL              5
  #define DEFAULT_I2C_CLOCK_SPEED          400000
  #define DEFAULT_WD_IC2_ADDRESS           38
#endif

#endif