// Include Arduino header
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <EEPROM.h>

#include "Rflink.h"

#define CFG_SSID_SIZE 				32
#define CFG_PSK_SIZE  				64
#define CFG_HOSTNAME_SIZE 			32

#define CFG_MQTT_SERVER_SIZE    	32
#define CFG_MQTT_USER_SIZE  		32
#define CFG_MQTT_PASSWORD_SIZE  	32

#ifdef LOAD_TEST
	#define FILTERED_ID_SIZE 		64
#else
	#ifndef FILTERED_ID_SIZE
		#define FILTERED_ID_SIZE 	32
		// Tested and works with 64 max (free mem left is ~6KB). Above it will exceed eeprom size and web interface may crash because of memory shortage 
	#endif
#endif

//********************************************************************************
// Structures definition
//********************************************************************************

// Structure for ID filtering
typedef struct {
  char id[MAX_ID_LEN+1];        			// ID
  char id_applied[MAX_ID_LEN+1];			// ID applied -> used for MQTT topic
  unsigned long publish_interval;        	// interval to publish (ms) if data did not change
  char description[MAX_DATA_LEN+1];         // Device description
} _filtered_IDs;

// Import user configuration
#include "config.h"

// Structure for MQTT configuration
typedef struct 
{
	char  			server[CFG_MQTT_SERVER_SIZE+1];     	// Broker 
	char  			user[CFG_MQTT_USER_SIZE+1]; 			// User
	char  			password[CFG_MQTT_PASSWORD_SIZE+1]; 	// Pass
	uint16_t 		port;                        			// Protocol port
} 	_mqtt;

// Structure for configuration
typedef struct { 											// structure to store configuration in EEPROM memory
	char			ssid[CFG_SSID_SIZE+1]; 		 			// SSID     
	char			psk[CFG_PSK_SIZE+1]; 		   			// Pre shared key
	char			hostname[CFG_HOSTNAME_SIZE+1]; 		   	// Hostname
	_mqtt			mqtt;                 					// MQTT configuration
	int				mega_reset_pin;							// Pin to reset MEGA
	unsigned long	resetMegaInterval;						// Auto reset MEGA
	bool			id_filtering;							// ID filtering enabled
	_filtered_IDs 	filtered_id[FILTERED_ID_SIZE];
	bool			settings_locked;						// Lock settings and block access point
	unsigned int 	version;
} 	_eepromConfig;


// Structure for matrix 
typedef struct { 											// structure for additionnal data when IP filtering is used
  char 				json[3*MAX_DATA_LEN]; 					// store last json message
  //uint16_t 		json_checksum;							// store last json checksum
  unsigned long 	last_published;    						// store last published time (millis)
  unsigned long 	last_received;     						// store last received time (millis)
} _matrix;

//********************************************************************************
// Constants and global variables
//********************************************************************************

const int filtered_id_number = min(FILTERED_ID_SIZE,(int) (sizeof(filtered_IDs) / sizeof(filtered_IDs[0]))); 



bool MQTT_DEBUG = 0;									// debug variable to publish received data on MQTT debug topic ; default is disabled, can be enabled from web interface
int mqttConnectionAttempts = 0;
#define MQTT_CHECK_INTERVAL 10000L						// check MQTT connection every 10s ; not too low to leave time for other functions (web interface, ...)	
unsigned long lastMqttConnect = 0;						// timer to check MQTT connection ; negative value forces it at startup

unsigned long lastReceived = 0;							// store last received data time from RFLink

#define UPTIME_INTERVAL 300000L							// publish uptime every 5 min
unsigned long lastUptime = 0;							// timer to publish uptime on MQTT server ; negative value forces update at startup

unsigned long now = 0;

int resetMegaCounter = 0;								// number of times Mega is reset

_eepromConfig eepromConfig;
const int eepromAddress = 0;												// Start address for eeprom config
const int eepromConfigMaxSize = max(3840, (int) sizeof(eepromConfig));		// Length for eeprom config - leaves some space

_matrix matrix[FILTERED_ID_SIZE];

// main input / output buffers
char BUFFER [BUFFER_SIZE];
char JSON   [BUFFER_SIZE];

// message builder buffers
char MQTT_NAME[MAX_DATA_LEN];
char MQTT_ID  [MAX_ID_LEN+1];
char MQTT_TOPIC[MAX_TOPIC_LEN];
char FIELD_BUF[MAX_DATA_LEN];

// Serial iterator counter
int CPT;

#ifdef EXPERIMENTAL
	// TEST lost packets
	char LINE_NUMBER[3];
	byte line_number = 0;
	int lost_packets = -1;
#endif

//********************************************************************************
// CSS and html in PROGMEM
//********************************************************************************

static const char cssDatasheet[] PROGMEM = ""     // CSS
	""  // from ESP Easy Mega release 20180914 - first part till @media-screen
	"* {font-family: sans-serif; font-size: 12pt; margin: 0px; padding: 0px; box-sizing: border-box; }h1 {font-size: 16pt; color: #07D; margin: 8px 0; font-weight: bold; }h2 {font-size: 12pt; margin: 0 -4px; padding: 6px; background-color: #444; color: #FFF; font-weight: bold; }h3 {font-size: 12pt; margin: 16px -4px 0 -4px; padding: 4px; background-color: #EEE; color: #444; font-weight: bold; }h6 {font-size: 10pt; color: #07D; }pre, xmp, code, kbd, samp, tt{ font-family:monospace,monospace; font-size:1em; }.button {margin: 4px; padding: 4px 16px; background-color: #07D; color: #FFF; text-decoration: none; border-radius: 4px; border: none;}.button.link { }.button.link.wide {display: inline-block; width: 100%; text-align: center;}.button.link.red {background-color: red;}.button.help {padding: 2px 4px; border-style: solid; border-width: 1px; border-color: gray; border-radius: 50%; }.button:hover {background: #369; }input, select, textarea {margin: 4px; padding: 4px 8px; border-radius: 4px; background-color: #eee; border-style: solid; border-width: 1px; border-color: gray;}input:hover {background-color: #ccc; }input.wide {max-width: 500px; width:80%; }input.widenumber {max-width: 500px; width:100px; }#selectwidth {max-width: 500px; width:80%; padding: 4px 8px;}select:hover {background-color: #ccc; }.container {display: block; padding-left: 35px; margin-left: 4px; margin-top: 0px; position: relative; cursor: pointer; font-size: 12pt; -webkit-user-select: none; -moz-user-select: none; -ms-user-select: none; user-select: none; }.container input {position: absolute; opacity: 0; cursor: pointer;  }.checkmark {position: absolute; top: 0; left: 0; height: 25px;  width: 25px;  background-color: #eee; border-style: solid; border-width: 1px; border-color: gray;  border-radius: 4px;}.container:hover input ~ .checkmark {background-color: #ccc; }.container input:checked ~ .checkmark { background-color: #07D; }.checkmark:after {content: ''; position: absolute; display: none; }.container input:checked ~ .checkmark:after {display: block; }.container .checkmark:after {left: 7px; top: 3px; width: 5px; height: 10px; border: solid white; border-width: 0 3px 3px 0; -webkit-transform: rotate(45deg); -ms-transform: rotate(45deg); transform: rotate(45deg); }.container2 {display: block; padding-left: 35px; margin-left: 9px; margin-bottom: 20px; position: relative; cursor: pointer; font-size: 12pt; -webkit-user-select: none; -moz-user-select: none; -ms-user-select: none; user-select: none; }.container2 input {position: absolute; opacity: 0; cursor: pointer;  }.dotmark {position: absolute; top: 0; left: 0; height: 26px;  width: 26px;  background-color: #eee; border-style: solid; border-width: 1px; border-color: gray; border-radius: 50%;}.container2:hover input ~ .dotmark {background-color: #ccc; }.container2 input:checked ~ .dotmark { background-color: #07D;}.dotmark:after {content: ''; position: absolute; display: none; }.container2 input:checked ~ .dotmark:after {display: block; }.container2 .dotmark:after {top: 8px; left: 8px; width: 8px; height: 8px; border-radius: 50%; background: white; }#toastmessage {visibility: hidden; min-width: 250px; margin-left: -125px; background-color: #07D;color: #fff;  text-align: center;  border-radius: 4px;  padding: 16px;  position: fixed;z-index: 1; left: 282px; bottom: 30%;  font-size: 17px;  border-style: solid; border-width: 1px; border-color: gray;}#toastmessage.show {visibility: visible; -webkit-animation: fadein 0.5s, fadeout 0.5s 2.5s; animation: fadein 0.5s, fadeout 0.5s 2.5s; }@-webkit-keyframes fadein {from {bottom: 20%; opacity: 0;} to {bottom: 30%; opacity: 0.9;} }@keyframes fadein {from {bottom: 20%; opacity: 0;} to {bottom: 30%; opacity: 0.9;} }@-webkit-keyframes fadeout {from {bottom: 30%; opacity: 0.9;} to {bottom: 0; opacity: 0;} }@keyframes fadeout {from {bottom: 30%; opacity: 0.9;} to {bottom: 0; opacity: 0;} }.level_0 { color: #F1F1F1; }.level_1 { color: #FCFF95; }.level_2 { color: #9DCEFE; }.level_3 { color: #A4FC79; }.level_4 { color: #F2AB39; }.level_9 { color: #FF5500; }.logviewer {  color: #F1F1F1; background-color: #272727;  font-family: 'Lucida Console', Monaco, monospace;  height:  530px; max-width: 1000px; width: 80%; padding: 4px 8px;  overflow: auto;   border-style: solid; border-color: gray; }textarea {max-width: 1000px; width:80%; padding: 4px 8px; font-family: 'Lucida Console', Monaco, monospace; }textarea:hover {background-color: #ccc; }table.normal th {padding: 6px; background-color: #444; color: #FFF; border-color: #888; font-weight: bold; }table.normal td {padding: 4px; height: 30px;}table.normal tr {padding: 4px; }table.normal {color: #000; width: 100%; min-width: 420px; border-collapse: collapse; }table.multirow th {padding: 6px; background-color: #444; color: #FFF; border-color: #888; font-weight: bold; }table.multirow td {padding: 4px; text-align: center;  height: 30px;}table.multirow tr {padding: 4px; }table.multirow tr:nth-child(even){background-color: #DEE6FF; }table.multirow {color: #000; width: 100%; min-width: 420px; border-collapse: collapse; }tr.highlight td { background-color: #dbff0075; }.note {color: #444; font-style: italic; }.headermenu {position: fixed; top: 0; left: 0; right: 0; height: 90px; padding: 8px 12px; background-color: #F8F8F8; border-bottom: 1px solid #DDD; z-index: 1;}.apheader {padding: 8px 12px; background-color: #F8F8F8;}.bodymenu {margin-top: 96px;}.menubar {position: inherit; top: 55px; }.menu {float: left; padding: 4px 16px 8px 16px; color: #444; white-space: nowrap; border: solid transparent; border-width: 4px 1px 1px; border-radius: 4px 4px 0 0; text-decoration: none; }.menu.active {color: #000; background-color: #FFF; border-color: #07D #DDD #FFF; }.menu:hover {color: #000; background: #DEF; }.menu_button {display: none;}.on {color: green; }.off {color: red; }.div_l {float: left; }.div_r {float: right; margin: 2px; padding: 1px 10px; border-radius: 4px; background-color: #080; color: white; }.div_br {clear: both; }.alert {padding: 20px; background-color: #f44336; color: white; margin-bottom: 15px; }.warning {padding: 20px; background-color: #ffca17; color: white; margin-bottom: 15px; }.closebtn {margin-left: 15px; color: white; font-weight: bold; float: right; font-size: 22px; line-height: 20px; cursor: pointer; transition: 0.3s; }.closebtn:hover {color: black; }section{overflow-x: auto; width: 100%; }\r\n"
	""	// force some changes in CSS compared to ESP Easy Mega
	"table.multirow td {text-align: center; font-size:0.9em;} table.multirow input {font-size:0.9em} h3 {margin: 16px 0px 0px 0px;}\r\n"
	"table.condensed td {padding: 0px 20px 0px 5px; height: 1em;}table.condensed tr {padding: 0px; }table.condensed {padding: 0px;border-left:1px solid #EEE;} .button.link {font-weight:normal;} #menunotification {margin-left: 2em;border-left: 1px solid #EEE;padding-left: 10px;padding-top:5px;} #menunotification td {min-width: 250px;} #configuration_table input[type=number] {width: 8em;} table.high td {height: 40px;} #cmds a {float:left;}\r\n"
	"table.multirow-left td {text-align: left;} table.multirow td.t-left, th.t-left {text-align: left;}\r\n"
	""  // from ESP Easy Mega release 20180914 - second part
	"@media screen and (max-width: 420) {span.showmenulabel { display: none; }.menu { max-width: 11vw; max-width: 48px; }\r\n";

static const char htmlMenu[] PROGMEM = ""     // Menu
	"<header class='headermenu'>\r\n"
	"<script>\r\n"
	"function fetchAndNotify(url) {\r\n"
	"	scroll(0,0);"
	"	fetch(url)\r\n"
	"		.then(function(response) {\r\n"
	"			return response.text();\r\n"
	"		 })\r\n"
	"		.then(function(text) {\r\n"
	"			console.log('Request successful: ', text);\r\n"
	"			document.getElementById('menunotification').innerHTML = text;\r\n"
	"		})\r\n"
	"		 .catch(function(error) {\r\n"
	"			log('Request failed', error);\r\n"
	"		 });\r\n"
	"	 };\r\n"
	"</script>\r\n"
	"<h1 id='menutitle'>espRFLinkMQTT</h1>\r\n"
	"<div class='menubar'>"
	"<a id='menuhome' class='menu' href='.'>&#8962;<span class='showmenulabel'>Home</span></a>\r\n"
	"<a id='menulivedata' class='menu' href='/livedata'>&#128172;<span class='showmenulabel'>Live Data</span></a>\r\n"
	//"<a id='menuresetmega' class='menu' href='/reset-mega' onclick = \"fetchAndNotify('/reset-mega');return false;\">&#128204;<span class='showmenulabel'>Reset MEGA</span></a>\r\n"
	//"<a id='menureboot' class='menu' href='/reboot' onclick = \"fetchAndNotify('/reboot');return false;\">&#128268;<span class='showmenulabel'>Reboot ESP</span></a>\r\n"
	"<a id='menuinfos' class='menu' href='/infos'>&#128295;<span class='showmenulabel'>System</span></a>\r\n"
	"</div>\r\n"
	"</header>\r\n"
	"<div id='menunotification' style='margin-left:2em;'></div>\r\n"
	"";

	static const char htmlStart[] PROGMEM = ""     // Html page start part
	"<!DOCTYPE html>\r\n<html>\r\n"
    "<head><title>espRFLinkMQTT</title>\r\n"
    "<meta name='viewport' content='width=device-width, initial-scale=1.0'>\r\n"
    "<meta http-equiv='Content-Type' content='text/html; charset=UTF-8' />\r\n"
	"<link rel=\"stylesheet\" type=\"text/css\" href=\"/esp.css\">\r\n"
    "</head>\r\n"
	"<body class='bodymenu'>\r\n";

	static const char htmlEnd[] PROGMEM = ""     // Html page end part
    "<br><footer><h6>Powered by <a href='https://github.com/seb821' style='font-size: 0.9em; text-decoration: none'>github.com/seb821</a></h6></footer>\r\n"
    "</body>\r\n</html>\r\n";

#ifdef RFLINK_WIFI_BOARD
	#include <Wire.h> // RFLInk WiFi board
	unsigned long lastSecond = 0;
#endif