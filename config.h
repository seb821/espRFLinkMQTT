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
HardwareSerial & rflinkSerialTX = Serial;		// rflinkSerialTX is used for data to RFLink
//#include <SoftwareSerial.h>					// rflinkSerialTX is used for data to RFLink: by default, it uses ESP TX pin ; in order to use another pin, comment previous line and uncomment this line and two following ones to write on software serial (GPIO2/D4) - software serial RX listens on GPIO4/D2 pin (unused)
//SoftwareSerial softSerial(4, 2, false);		// software serial TX to RFLink on GPIO2/D4 ; software serial RX from GPIO4/D2 pin (unused) 
//SoftwareSerial & rflinkSerialTX = softSerial;          

#define MEGA_RESET_PIN 16						// ESP pin connected to MEGA reset pin - GPIO16 = D0
#define MEGA_AUTO_RESET_INTERVAL 0 * 1000 * 60	// Auto reset Mega if no data is received during this period of time (in ms) - 0 to disable


//********************************************************************************
// Wi-Fi parameters
//********************************************************************************


#define WIFI_SSID "XXXXXX"				// network SSID for ESP8266 to connect to
#define WIFI_PASSWORD "XXXXXX"			// password for the network above

#define CLIENT_NAME "espRFLinkMQTT" 			// Client name used for titles, hostname, OTA, client ID for MQTT server


//********************************************************************************
// MQTT parameters
//********************************************************************************

#define ENABLE_MQTT_SETTINGS_ONLINE_CHANGE		// If defined, the settings below can be changed in the webinterface

#define MQTT_SERVER "192.168.1.10"				// MQTT Server
#define MQTT_PORT 1883							// MQTT server port
#define MQTT_USER ""							// MQTT Server user
#define MQTT_PASSWORD ""						// MQTT Server password

#define MQTT_PUBLISH_TOPIC "rflink"				// MQTT topic to publish to
#define MQTT_RFLINK_CMD_TOPIC "rflink/cmd"		// MQTT topic to listen to for rflink order
#define MQTT_RETAIN_FLAG false					// If true, messages will be published with the retain flag
#define MQTT_WILL_TOPIC "rflink/online"			// MQTT last will topic ; "rflink/status"
#define MQTT_WILL_ONLINE "1"					// MQTT last will topic value online; "online"
#define MQTT_WILL_OFFLINE "0"					// MQTT last will topic value offline; "offline"

#define MQTT_DEBUG_TOPIC "rflink/debug"			// MQTT debug topic to publish raw data, name, ID, MQTT topic and json (json format)
#define MQTT_UPTIME_TOPIC "rflink/uptime"		// MQTT topic for uptime

#define MQTT_MEGA_RESET_TOPIC "rflink/mega_reset"	// MQTT topic whereto publish a 1s pulse before resetting RFLink Mega


//********************************************************************************
// ID filtering
//********************************************************************************

#define ID_FILTERING	false	// If set to false, by default there is no ID filtering: everything will be published on MQTT server. If set to true, it enables ID filtering by default. This can now be changed from the web interface.

const _filtered_IDs filtered_IDs[] = {
	// This table allows to configure default values for IDs that will be filtered on before being sent to MQTT server.
	// First column is ID that will be filtered for.
	// Second column is ID applied for MQTT topic.
	// Third column is interval time (in ms) to force publication: if data received has not changed within this interval time, it will not be published (note that if data received changed it is always published). Use 0 to publish everytime some data is received
	// Last column is a description (24 characters max).
	// This configuration can now be modified online later on. Maximum number of devices is 32.
		{"1082","1082",1800000,"Auriol V3"},					//1
		{"0210","0210",1800000,"Alecto V5"},					//2
		{"2A04","2A1C",1800000,"Oregon Rain2"},					//3
		{"00000","00000",180000,"device #4 30 min update"},		// 4
		{"00000","00000",0000,"device #5 always updated"},		// 5
		{"00000","00000",1000,"device #6 1s update"},			// 6
		{"00000","00000",1000,"device #7"},						// 7
		{"00000","00000",1000,"device #8"},						// 8
		{"00000","00000",1000,"device #9"},						// 9
		{"00000","00000",1000,"device #10"},					// 10
		{"00000","00000",1000,"device #11"},					// 11
		{"00000","00000",1000,"device #12"},					// 12
		{"00000","00000",1000,"device #13"},					// 13
		{"00000","00000",1000,"device #14"},					// 14
		{"00000","00000",1000,"device #15"},					// 15
		{"00000","00000",1000,"device #16"},					// 16
		{"00000","00000",1000,"device #17"},					// 17
		{"00000","00000",1000,"device #18"},					// 18
		{"00000","00000",1000,"device #19"},					// 19
		{"00000","00000",1000,"device #20"},					// 20
		{"00000","00000",1000,"device #21"},					// 21
		{"00000","00000",1000,"device #22"},					// 22
		{"00000","00000",1000,"device #23"},					// 23
		{"00000","00000",1000,"device #24"},					// 24
		{"00000","00000",1000,"device #25"},					// 25
		{"00000","00000",1000,"device #26"},					// 26
		{"00000","00000",1000,"device #27"},					// 27
		{"00000","00000",1000,"device #28"},					// 28
		{"00000","00000",1000,"device #29"},					// 29
		{"00000","00000",1000,"device #30"},					// 30
		{"00000","00000",1000,"device #31"},					// 31
		{"00000","00000",1000,"device #32"},					// 32
};
// Note: ID filtering configuration is now saved in eeprom memory and can be changed online in the /configuration page. This is very useful when a device changes ID in order to keep using the same MQTT topic. If changes are made online, the /configuration page provides directly the new code to update here.

#define CONFIG_VERSION 20200406	// ! => Changing this number overwrites the configuration in eeprom memory with configuration in this file (config.h). In general you should always update this number after making changes here.

/*
This is how CONFIG_VERSION works :
- at startup software reads config version in eeprom memory
- if value is the same as above, it uses eeprom memory configuration
- if it is different, it overwrites eeprom memory configuration with configuration contained in this file (config.h)
This means that if changes were made online (Wifi and MQTT settings, ID filtering configuration), it is safer to update configuration  in this file not to lose changes. 
*/

const char* const user_specific_ids[][3] = {    // This is used to force a specific ID for devices changing frequently ; it applies to a specific protocol to all IDs (put ALL) or to a specific ID
  //{ "Auriol_V3"  ,  "ALL"  , "1082"   },
  //{ "Alecto_V5"  ,  "0001" , "0210"   },
};


//********************************************************************************
// Interface parameters
//********************************************************************************

const char* const user_cmds[][2] = {			// These are predefined user commands to RFLink appearing in the web interface
  { "Ping"       ,  "10;ping;"                              },
  { "Version"    ,  "10;version;"                           },
  { "Status"     ,  "10;status;"                            },
  { "Reboot"     ,  "10;reboot;"                            },
};

#endif