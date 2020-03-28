
// Configuration is to be done in config.h file

#define BUFFER_SIZE 200                         // Maximum size of one serial line and json content
#define MAX_ID_LEN 12                         	// Maximum size for record name, record id, json field name and json field value
#define MAX_DATA_LEN 24                         // Maximum size for record name, record id, json field name and json field value
#define MAX_TOPIC_LEN 60                        // Maximum topic path size (at least lenght of MQTT_PUBLISH_TOPIC + 2 x MAX_DATA_LEN)

struct USER_ID_STRUCT {
  char id[MAX_ID_LEN];        	// ID
  char id_applied[MAX_ID_LEN];	// ID applied
  long publish_interval;        	// interval to publish (ms) if data did not change
  char description[MAX_DATA_LEN+1];         	// description
};

struct USER_SPECIFIC_ID_STRUCT {      // This is used to force a specific ID for devices changing frequently ; it applies to a specific name/protocol
  char name[MAX_DATA_LEN];            // Protocol (name)
  char id_filtered[MAX_ID_LEN];     // ID sent by the device ; choose ALL to filter only on the protocol => in this case, there should be only one device sending data with the previous protocol
  char id_applied[MAX_ID_LEN];      // ID applied
};

struct USER_CMD_STRUCT {        // Preconfigured commands to show on web interface
  char text[MAX_DATA_LEN];      // button text
  char command[BUFFER_SIZE];    // command to send
};

#include "config.h"
#include "ArduinoJson.h"
#include "Rflink.h"
#include <cont.h>

/*********************************************************************************
 * Global Variables
/*********************************************************************************/

#if defined(SERIAL_DEBUG) /** Serial debug functions */
	//#define DEBUG_BEGIN(x)   	debugSerialTX.begin(x);
	#define DEBUG_PRINT(x)   	debugSerialTX.print(x);
	#define DEBUG_PRINTF(x,y)  	debugSerialTX.printf(x,y);
	#define DEBUG_PRINTLN(x) 	debugSerialTX.println(x);
	#define DEBUG_WRITE(x,y)   	debugSerialTX.write(x,y);
#else
	//#define DEBUG_BEGIN(x)   	{};
	#define DEBUG_PRINT(x)   	{};
	#define DEBUG_PRINTF(x,y)  	{};
	#define DEBUG_PRINTLN(x) 	{};
	#define DEBUG_WRITE(x, y) 	{};
#endif
  
bool MQTT_DEBUG = 0;		// debug variable to publish received data on MQTT debug topic ; default is disabled, can be enabled from web interface
long lastUptime = - 5 * 1000 * 60;			      // timer to publish uptime on MQTT server ; sufficient negative value forces update at startup
const long uptimeInterval = 5 * 1000 * 60;		      // publish uptime every 5 min
const long resetMegaInterval = AUTO_RESET_MEGA_INTERVAL; // auto reset Mega if no data is received during more than x min - 0 to disable
int resetMegaCounter = 0;                     // number of times Mega is reset
long now;
long lastReceived = millis();     			      // store last received data time (millis)

struct USER_ID_STRUCT_MATRIX {
  //char id[MAX_ID_LEN];			// ID
  //char id_applied[MAX_ID_LEN];	// ID applied
  //long publish_interval;  			// interval to publish (ms) if data did not change
  //char description[60];    			// description
  char json[3*MAX_DATA_LEN]; 			// store last json message
  //uint16_t json_checksum;			// store last json checksum
  long last_published;    			// store last publish (millis)
  long last_received;     			// store last received (millis)
};
USER_ID_STRUCT_MATRIX matrix[USER_ID_NUMBER];

// structure to store USER_IDs configuration in EEPROM memory
struct eepromConfig_struct {
  unsigned int version;
  USER_ID_STRUCT user_id[USER_ID_NUMBER];
};
eepromConfig_struct eepromConfig;
const int eepromAddress = 0;   // Start address for eeprom config
const int eepromLength = 4096;   // Lenght for eeprom config

/** Save in EEPROM memory current configuration for USER_IDs*/
void saveEEPROM() {
  EEPROM.begin(eepromLength);
  // Update version number before writing eeprom
  eepromConfig.version = VERSION;
  EEPROM.put(eepromAddress, eepromConfig);
  EEPROM.commit();
  EEPROM.end();
  DEBUG_PRINTLN("Config saved to EEPROM");
}

/** Load USER_IDs from EEPROM memory if version matches */
void loadEEPROM() {
  EEPROM.begin(eepromLength);
  DEBUG_PRINT("EEPROM size: ");DEBUG_PRINTLN(EEPROM.length());
  EEPROM.get(eepromAddress,eepromConfig);  
  // Check if program version matches with eeprom version
  bool version_changed = eepromConfig.version != VERSION;
  DEBUG_PRINT("Version in EEPROM: ")DEBUG_PRINTLN(eepromConfig.version);
  DEBUG_PRINT("Version in config.h: ")DEBUG_PRINTLN(VERSION);
  if (version_changed) {
    DEBUG_PRINTLN("Version changed - initialize EEPROM configuration with USER_IDs from config.h");
      for (int i =0; i < USER_ID_NUMBER; i++) {
          strcpy(eepromConfig.user_id[i].id,USER_IDs[i].id);
          strcpy(eepromConfig.user_id[i].id_applied,USER_IDs[i].id_applied);
          eepromConfig.user_id[i].publish_interval = USER_IDs[i].publish_interval;
          strcpy(eepromConfig.user_id[i].description,USER_IDs[i].description);
      }
    saveEEPROM();
  }
  else DEBUG_PRINTLN("Version did not change - using EEPROM configuration for USER_IDs");
  EEPROM.end();
  DEBUG_PRINTLN("EEPROM content for USER_IDs:");
  for (int i =0; i < USER_ID_NUMBER; i++) {
    DEBUG_PRINT("   ");DEBUG_PRINT(eepromConfig.user_id[i].id);
    DEBUG_PRINT(" - ");DEBUG_PRINT(eepromConfig.user_id[i].id_applied);	
    DEBUG_PRINT(" - ");DEBUG_PRINT(eepromConfig.user_id[i].publish_interval);
    DEBUG_PRINT(" - ");DEBUG_PRINT(eepromConfig.user_id[i].description);
    DEBUG_PRINTLN();
  }
}

// main input / output buffers
char BUFFER [BUFFER_SIZE];
char BUFFER_DEBUG [10 + BUFFER_SIZE + 10 + MAX_ID_LEN + 10 + MAX_DATA_LEN + 10]; // It may be necessary to increase MQTT_MAX_PACKET_SIZE in PubSubClient.h

char JSON   [BUFFER_SIZE];
char JSON_DEBUG   [BUFFER_SIZE];

// message builder buffers
char MQTT_NAME[MAX_DATA_LEN];
char MQTT_ID  [MAX_ID_LEN];
char MQTT_TOPIC[MAX_TOPIC_LEN];
char FIELD_BUF[MAX_DATA_LEN];

// Serial iterator counter
int CPT;

/**
 * Manage Wifi and MQTT server connection
 */
 
WiFiClient espClient;
PubSubClient MQTTClient(MQTT_SERVER, MQTT_PORT, callback, espClient);

ESP8266WebServer httpserver(80);                               // Create a webserver object that listens for HTTP request on port 80
ESP8266HTTPUpdateServer httpUpdater;                           // Firmware webupdate

void setup_wifi() {
        delay(10);
        DEBUG_PRINT("Connecting to ");
        DEBUG_PRINT(WIFI_SSID); DEBUG_PRINTLN(" ...");
        WiFi.hostname(MQTT_RFLINK_CLIENT_NAME);
		WiFi.mode(WIFI_STA); 											// Act as wifi_client only, defaults to act as both a wifi_client and an access-point.
        WiFi.begin(WIFI_SSID,WIFI_PASSWORD);                            // Connect to the network
        int i = 0;
        while (WiFi.status() != WL_CONNECTED) {                         // Wait for the Wi-Fi to connect
          delay(1000);
          DEBUG_PRINT(++i); DEBUG_PRINT(' ');
        }
        DEBUG_PRINTLN();
        DEBUG_PRINTLN("WiFi connected");
        DEBUG_PRINT("IP address:\t ");
        DEBUG_PRINTLN(WiFi.localIP());
}

/**
 * callback to handle rflink order received from MQTT subscribtion
 */
 
void callback(char* topic, byte* payload, unsigned int len) {
        rflinkSerialTX.write(payload, len);
        rflinkSerialTX.print(F("\r\n"));
        DEBUG_PRINTLN(F("=== MQTT command ==="));
        DEBUG_PRINT(F("message = "));
        DEBUG_WRITE(payload, len);
        DEBUG_PRINT(F("\r\n"));
}

/**
 * build MQTT topic name to pubish to using parsed NAME and ID from rflink message
 */
 
void buildMqttTopic() {
        MQTT_TOPIC[0] = '\0';
        strcpy(MQTT_TOPIC,MQTT_PUBLISH_TOPIC);
        strcat(MQTT_TOPIC,"/");
        strcat(MQTT_TOPIC,MQTT_NAME);
        strcat(MQTT_TOPIC,"-");
        strcat(MQTT_TOPIC,MQTT_ID);;
        //strcat(MQTT_TOPIC,"\0");
}

/**
 * send formated message to serial
 */
 
void printToSerial() {
        DEBUG_PRINTLN();
        DEBUG_PRINTLN(F("=== RFLink packet ==="));
        DEBUG_PRINT(F("Raw data = ")); DEBUG_PRINT(BUFFER);
        DEBUG_PRINT(F("MQTT topic = "));
        DEBUG_PRINT(MQTT_TOPIC);
        DEBUG_PRINT("/ => ");
        DEBUG_PRINTLN(JSON);
        DEBUG_PRINTLN();
}

/**
 * try to connect to MQTT Server
 */
 
boolean MqttConnect() {

        // connect to Mqtt server and subcribe to order topic
        if (MQTTClient.connect(MQTT_RFLINK_CLIENT_NAME, MQTT_USER, MQTT_PASSWORD, MQTT_WILL_TOPIC, 0, 1, MQTT_WILL_OFFLINE)) {
                MQTTClient.subscribe(MQTT_RFLINK_ORDER_TOPIC);
                MQTTClient.publish(MQTT_WILL_TOPIC,MQTT_WILL_ONLINE,1);   // once connected, update status of will topic
        }

        // report mqtt connection status
        DEBUG_PRINT(F("MQTT connection state : "));
        DEBUG_PRINTLN(MQTTClient.state());
        return MQTTClient.connected();
}

/**
 * OTA
 */
 
void SetupOTA() {

  // ArduinoOTA.setPort(8266);                                      // Port defaults to 8266
  ArduinoOTA.setHostname(MQTT_RFLINK_CLIENT_NAME);                  // Hostname defaults to esp8266-[ChipID]
  
  ArduinoOTA.onStart([]() {
    DEBUG_PRINTLN("Start OTA");
  });

  ArduinoOTA.onEnd([]() {
    DEBUG_PRINTLN("End OTA");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    DEBUG_PRINTF("Progress: %u%%\n", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    DEBUG_PRINTF("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      DEBUG_PRINTLN("Auth Failed");
    }
    else if (error == OTA_BEGIN_ERROR) {
      DEBUG_PRINTLN("Begin Failed");
    }
    else if (error == OTA_CONNECT_ERROR) {
      DEBUG_PRINTLN("Connect Failed");
    }
    else if (error == OTA_RECEIVE_ERROR) {
      DEBUG_PRINTLN("Receive Failed");
    }
    else if (error == OTA_END_ERROR) {
      DEBUG_PRINTLN("End Failed");
    }
  });

};
 
/**
 * HTTP server
 */
  
const char cssEspEasy[] = ""     // CSS
	""     // from ESP Easy Mega release 20180914
	"* {font-family: sans-serif; font-size: 12pt; margin: 0px; padding: 0px; box-sizing: border-box; }h1 {font-size: 16pt; color: #07D; margin: 8px 0; font-weight: bold; }h2 {font-size: 12pt; margin: 0 -4px; padding: 6px; background-color: #444; color: #FFF; font-weight: bold; }h3 {font-size: 12pt; margin: 16px -4px 0 -4px; padding: 4px; background-color: #EEE; color: #444; font-weight: bold; }h6 {font-size: 10pt; color: #07D; }pre, xmp, code, kbd, samp, tt{ font-family:monospace,monospace; font-size:1em; }.button {margin: 4px; padding: 4px 16px; background-color: #07D; color: #FFF; text-decoration: none; border-radius: 4px; border: none;}.button.link { }.button.link.wide {display: inline-block; width: 100%; text-align: center;}.button.link.red {background-color: red;}.button.help {padding: 2px 4px; border-style: solid; border-width: 1px; border-color: gray; border-radius: 50%; }.button:hover {background: #369; }input, select, textarea {margin: 4px; padding: 4px 8px; border-radius: 4px; background-color: #eee; border-style: solid; border-width: 1px; border-color: gray;}input:hover {background-color: #ccc; }input.wide {max-width: 500px; width:80%; }input.widenumber {max-width: 500px; width:100px; }#selectwidth {max-width: 500px; width:80%; padding: 4px 8px;}select:hover {background-color: #ccc; }.container {display: block; padding-left: 35px; margin-left: 4px; margin-top: 0px; position: relative; cursor: pointer; font-size: 12pt; -webkit-user-select: none; -moz-user-select: none; -ms-user-select: none; user-select: none; }.container input {position: absolute; opacity: 0; cursor: pointer;  }.checkmark {position: absolute; top: 0; left: 0; height: 25px;  width: 25px;  background-color: #eee; border-style: solid; border-width: 1px; border-color: gray;  border-radius: 4px;}.container:hover input ~ .checkmark {background-color: #ccc; }.container input:checked ~ .checkmark { background-color: #07D; }.checkmark:after {content: ''; position: absolute; display: none; }.container input:checked ~ .checkmark:after {display: block; }.container .checkmark:after {left: 7px; top: 3px; width: 5px; height: 10px; border: solid white; border-width: 0 3px 3px 0; -webkit-transform: rotate(45deg); -ms-transform: rotate(45deg); transform: rotate(45deg); }.container2 {display: block; padding-left: 35px; margin-left: 9px; margin-bottom: 20px; position: relative; cursor: pointer; font-size: 12pt; -webkit-user-select: none; -moz-user-select: none; -ms-user-select: none; user-select: none; }.container2 input {position: absolute; opacity: 0; cursor: pointer;  }.dotmark {position: absolute; top: 0; left: 0; height: 26px;  width: 26px;  background-color: #eee; border-style: solid; border-width: 1px; border-color: gray; border-radius: 50%;}.container2:hover input ~ .dotmark {background-color: #ccc; }.container2 input:checked ~ .dotmark { background-color: #07D;}.dotmark:after {content: ''; position: absolute; display: none; }.container2 input:checked ~ .dotmark:after {display: block; }.container2 .dotmark:after {top: 8px; left: 8px; width: 8px; height: 8px; border-radius: 50%; background: white; }#toastmessage {visibility: hidden; min-width: 250px; margin-left: -125px; background-color: #07D;color: #fff;  text-align: center;  border-radius: 4px;  padding: 16px;  position: fixed;z-index: 1; left: 282px; bottom: 30%;  font-size: 17px;  border-style: solid; border-width: 1px; border-color: gray;}#toastmessage.show {visibility: visible; -webkit-animation: fadein 0.5s, fadeout 0.5s 2.5s; animation: fadein 0.5s, fadeout 0.5s 2.5s; }@-webkit-keyframes fadein {from {bottom: 20%; opacity: 0;} to {bottom: 30%; opacity: 0.9;} }@keyframes fadein {from {bottom: 20%; opacity: 0;} to {bottom: 30%; opacity: 0.9;} }@-webkit-keyframes fadeout {from {bottom: 30%; opacity: 0.9;} to {bottom: 0; opacity: 0;} }@keyframes fadeout {from {bottom: 30%; opacity: 0.9;} to {bottom: 0; opacity: 0;} }.level_0 { color: #F1F1F1; }.level_1 { color: #FCFF95; }.level_2 { color: #9DCEFE; }.level_3 { color: #A4FC79; }.level_4 { color: #F2AB39; }.level_9 { color: #FF5500; }.logviewer {  color: #F1F1F1; background-color: #272727;  font-family: 'Lucida Console', Monaco, monospace;  height:  530px; max-width: 1000px; width: 80%; padding: 4px 8px;  overflow: auto;   border-style: solid; border-color: gray; }textarea {max-width: 1000px; width:80%; padding: 4px 8px; font-family: 'Lucida Console', Monaco, monospace; }textarea:hover {background-color: #ccc; }table.normal th {padding: 6px; background-color: #444; color: #FFF; border-color: #888; font-weight: bold; }table.normal td {padding: 4px; height: 30px;}table.normal tr {padding: 4px; }table.normal {color: #000; width: 100%; min-width: 420px; border-collapse: collapse; }table.multirow th {padding: 6px; background-color: #444; color: #FFF; border-color: #888; font-weight: bold; }table.multirow td {padding: 4px; text-align: center;  height: 30px;}table.multirow tr {padding: 4px; }table.multirow tr:nth-child(even){background-color: #DEE6FF; }table.multirow {color: #000; width: 100%; min-width: 420px; border-collapse: collapse; }tr.highlight td { background-color: #dbff0075; }.note {color: #444; font-style: italic; }.headermenu {position: fixed; top: 0; left: 0; right: 0; height: 90px; padding: 8px 12px; background-color: #F8F8F8; border-bottom: 1px solid #DDD; z-index: 1;}.apheader {padding: 8px 12px; background-color: #F8F8F8;}.bodymenu {margin-top: 96px;}.menubar {position: inherit; top: 55px; }.menu {float: left; padding: 4px 16px 8px 16px; color: #444; white-space: nowrap; border: solid transparent; border-width: 4px 1px 1px; border-radius: 4px 4px 0 0; text-decoration: none; }.menu.active {color: #000; background-color: #FFF; border-color: #07D #DDD #FFF; }.menu:hover {color: #000; background: #DEF; }.menu_button {display: none;}.on {color: green; }.off {color: red; }.div_l {float: left; }.div_r {float: right; margin: 2px; padding: 1px 10px; border-radius: 4px; background-color: #080; color: white; }.div_br {clear: both; }.alert {padding: 20px; background-color: #f44336; color: white; margin-bottom: 15px; }.warning {padding: 20px; background-color: #ffca17; color: white; margin-bottom: 15px; }.closebtn {margin-left: 15px; color: white; font-weight: bold; float: right; font-size: 22px; line-height: 20px; cursor: pointer; transition: 0.3s; }.closebtn:hover {color: black; }section{overflow-x: auto; width: 100%; }@media screen and (max-width: 960px) {span.showmenulabel { display: none; }.menu { max-width: 11vw; max-width: 48px; }\r\n";
  
const char cssDatasheet[] = ""     // CSS
	""	// force some changes in CSS compared to ESP Easy Mega
	"<style>table.multirow td {text-align: center; font-size:0.9em;} table.multirow input {font-size:0.9em} h3 {margin: 16px 0px 0px 0px;}\r\n"
	"table.condensed td {padding: 0px 20px 0px 5px; height: 1em;}table.condensed tr {padding: 0px; }table.condensed {padding: 0px;border-left:1px solid #EEE;}\r\n"
	"table.multirow-left td {text-align: left;}\r\n"
	"</style>\r\n";
	
void ConfigHTTPserver() {

  httpserver.on("/esp.css",[](){       // CSS EspEasy
	DEBUG_PRINTLN("Html page requested: /esp.css");
	
    String cssMessage = String(cssEspEasy);
	httpserver.sendHeader("Access-Control-Max-Age", "86400");
	httpserver.send(200, "text/css", cssMessage);
	
  });
	
  httpserver.on("/",[](){       // Main, page d'accueil
	DEBUG_PRINTLN("Html page requested: /");
	
    String htmlMessage = "<!DOCTYPE html>\r\n<html>\r\n";
	
	// URL arguments
	int page = 0;
	String arg_page = httpserver.arg("page");
	if (arg_page.length() > 0) {
		page = max((int) arg_page.toInt(),0);
	};
	
    // Head
    htmlMessage += "<head><title>" + String(MQTT_RFLINK_CLIENT_NAME) + "</title>\r\n";
    htmlMessage += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>\r\n";
    htmlMessage += "<meta http-equiv='Content-Type' content='text/html; charset=UTF-8' />\r\n";
	htmlMessage += "<link rel=\"stylesheet\" type=\"text/css\" href=\"esp.css\">\r\n";
	htmlMessage += String(cssDatasheet);	// CSS
    htmlMessage += "</head>\r\n";

    // Body
    htmlMessage += "<body class='bodymenu'>\r\n";

    // Header + menu
    htmlMessage += "<header class='headermenu'>";
    htmlMessage += "<h1>" + String(MQTT_RFLINK_CLIENT_NAME) + "</h1>\r\n";
    htmlMessage += "<div class='menubar'><a class='menu active' href='.'>&#8962;<span class='showmenulabel'>Main</span></a>\r\n";
    htmlMessage += "<a class='menu' href='/livedata'>&#10740;<span class='showmenulabel'>Live Data</span></a>\r\n";
    htmlMessage += "<a id='resetmega' class='menu' href='/reset-mega' onclick = \"fetch('/reset-mega');return false;\">&#128204;<span class='showmenulabel'>Reset MEGA</span></a>\r\n";
    htmlMessage += "<a class='menu' id='reboot' href='/reboot'>&#128268;<span class='showmenulabel'>Reboot ESP</span></a>\r\n";
    htmlMessage += (!MQTT_DEBUG)? "<a class='menu' href='/enable-debug'>&#128172;<span class='showmenulabel'>Enable MQTT debug</span></a>\r\n":"<a class='menu' href='/disable-debug'>&#128172;<span class='showmenulabel'>Disable MQTT debug</span></a>\r\n";
    htmlMessage += "<a class='menu' href='/infos'>&#128295;<span class='showmenulabel'>System Info</span></a></div>\r\n";
    htmlMessage += "</header>\r\n";	
    
    // Live data
    htmlMessage += "<h3>RFLink Live Data *</h3>\r\n";

    htmlMessage += "<input type=\"button\" value =\"Pause\" onclick=\"stopUpdate();\" />";                                                                // Pause
    htmlMessage += "<input type=\"button\" value =\"Restart\" onclick=\"restartUpdate();\" />";                                                           // Restart
    htmlMessage += "<input type=\"button\" value =\"Refresh\" onclick=\"window.location.reload(true);\" />\r\n";                                          // Refresh
    
    htmlMessage += "<table id=\"liveData\" class='multirow multirow-left';>\r\n";                                                                         // Table of x lines
    htmlMessage += "<tr class=\"header\"><th style='text-align:left;'>Raw Data</th><th style='text-align:left;'> MQTT Topic </th><th style='text-align:left;'> MQTT JSON </th></tr>\r\n";
    for (int i = 0; i < (7); i++){ // not too high to avoid overflow
      htmlMessage += "<tr id=\"data" + String(i) + "\"><td></td><td></td><td></td></tr>\r\n";
    } 
    htmlMessage += "</table>\r\n";
    
    htmlMessage += "<script>\r\n";                                                        // Script to update data and move to next line
    htmlMessage += "var x = setInterval(function() {loadData(\"data.txt\",updateData)}, 500);\r\n";       // update every 500 ms
    htmlMessage += "function loadData(url, callback){\r\n";
    htmlMessage += "var xhttp = new XMLHttpRequest();\r\n";
    htmlMessage += "xhttp.onreadystatechange = function(){\r\n";
    htmlMessage += " if(this.readyState == 4 && this.status == 200){\r\n";
    htmlMessage += " callback.apply(xhttp);\r\n";
    htmlMessage += " }\r\n";
    htmlMessage += "};\r\n";
    htmlMessage += "xhttp.open(\"GET\", url, true);\r\n";
    htmlMessage += "xhttp.send();\r\n";
    htmlMessage += "}\r\n";
    htmlMessage += "var memorized_data;\r\n";
    htmlMessage += "function updateData(){\r\n";
    htmlMessage += "if (memorized_data != this.responseText) {\r\n";
    for (int i = (7 - 1); i > 0; i--){	// not too high to avoid overflow
      htmlMessage += "document.getElementById(\"data" + String(i) + "\").innerHTML = document.getElementById(\"data" + String (i-1) + "\").innerHTML;\r\n";
    }
    htmlMessage += "}\r\n";
    htmlMessage += "document.getElementById(\"data0\").innerHTML = this.responseText;\r\n";
    htmlMessage += "memorized_data = this.responseText;\r\n"; // memorize new data
    htmlMessage += "}\r\n";
    htmlMessage += "function stopUpdate(){\r\n";
    htmlMessage += " clearInterval(x);\r\n";
    htmlMessage += "}\r\n";
    htmlMessage += "function restartUpdate(){\r\n";
    htmlMessage += " x = setInterval(function() {loadData(\"data.txt\",updateData)}, 500);\r\n";       // update every 500 ms
    htmlMessage += "}\r\n";
    htmlMessage += "</script>\r\n";
    htmlMessage += "<div class='note'>* see Live \"Data tab\" for more lines - web view may not catch all frames, MQTT debug is more accurate</div>\r\n";
    
    // Commands to RFLink
    htmlMessage += "<h3>Commands to RFLink</h3><br />";
    htmlMessage += "<form action=\"/send\" id=\"form_command\" style=\"float: left;\"><input type=\"text\" id=\"command\" name=\"command\">";
    htmlMessage += "<input type=\"submit\" value=\"Send\"><a class='button help' href='http://www.rflink.nl/blog2/protref' target='_blank'>&#10068;</a></form>\r\n";
	htmlMessage += "<script>function submitForm() {"
    "var url = '/send';"
    "var formData = new FormData();"
    "formData.append(\"command\", document.getElementById('command').value);"
    "var fetchOptions = {"
    "  method: 'POST',"
    "  headers : new Headers(),"
    "  body: formData"
    "};"
    "fetch(url, fetchOptions);"
		"}</script>";
    for (int i = 0; i < (sizeof(USER_CMDs) / sizeof(USER_CMDs[0])); i++){      // User commands defined in USER_CMDs for quick access
      htmlMessage += "<a class='button link' style=\"float: left;\" href=\"javascript:void(0)\" "; 
      htmlMessage += "onclick=\"document.getElementById('command').value = '" + String(USER_CMDs[i].command) + "';";
      htmlMessage += "submitForm(); return false;\">" + String(USER_CMDs[i].text) + "</a>\r\n";  
	}
    htmlMessage += "<br style=\"clear: both;\"/>\r\n";
	
    // User ID
    if (USER_ID_NUMBER > 0) {                                                                         // show list of user IDs
		htmlMessage += "<a id=\"userids\"/></a><br><br>\r\n";
		htmlMessage += "<h3 id='configuration'>User IDs * \r\n";
	
		if (USER_ID_NUMBER > 16) {
			for (int i = 0; i <= ((int) (USER_ID_NUMBER-1)/16); i++){
			   htmlMessage += "<a href='/?page=" + String(i) + "#userids' class='button link'>" + String(i*16+1) + " - " + String(min(USER_ID_NUMBER,i*16+16)) + "</a>\r\n";
			}
		}
		
		htmlMessage += " <a class='button link' href='/configuration'>Change</a></h3><br>\r\n";
	
      int i;
      htmlMessage += "<table class='multirow'><tr><th>#</th><th>ID</th><th>ID applied</th><th>Description</th><th>Interval</th><th>Received</th><th>Published</th><th style='text-align:left;'>Last MQTT JSON</th></tr>\r\n";
      //for (i = 0; i < (USER_ID_NUMBER); i++){
	  for (int i = 0+(16*page); i < min(USER_ID_NUMBER,16*(page+1)); i++){
        if (strcmp(eepromConfig.user_id[i].id,"") != 0) {
          htmlMessage += "<tr><td>" + String(i+1) + "</td><td>" + String(eepromConfig.user_id[i].id) + "</td>";
          htmlMessage += "<td>" + String(eepromConfig.user_id[i].id_applied) + "</td>";
          htmlMessage += "<td style=\"text-align: left;\">" + String(eepromConfig.user_id[i].description) + "</td>";
          htmlMessage += "<td>" + String( int(float(eepromConfig.user_id[i].publish_interval) *0.001 /60)) + " min</td>";
          if (matrix[i].last_received != 0) {
  		  htmlMessage += "<td>" + time_string_exp(now - matrix[i].last_received) + "</td>";
          } else {
            htmlMessage += "<td></td>";
          }
          if (matrix[i].last_published != 0) {
            htmlMessage += "<td>" + time_string_exp(now - matrix[i].last_published) + "</td>";
          } else {
            htmlMessage += "<td></td>";
          }
          htmlMessage += "<td style='text-align: left;'>" + String(matrix[i].json) + "</td></tr>\r\n";

        }
      }
      htmlMessage += "</table>\r\n";
      htmlMessage += "<div class='note'>* only those IDs are published on MQTT server, and only if data changed or interval time is exceeded</div>\r\n";
    } else {
      htmlMessage += "<div class='note'>* all IDs are forwarded to MQTT server</div>\r\n";
    }

	htmlMessage += "<br><br><span style=\"font-size: 0.9em\">Running for " + time_string_exp(millis()) + " - Free Mem " + String (ESP.getFreeHeap()) + "</span>\r\n";
	htmlMessage += "<input type=\"button\" value =\"Refresh\" onclick=\"window.location.reload(true);\" />\r\n";
    htmlMessage += "<footer><br><h6>Powered by <a href='https://github.com/seb821' style='font-size: 15px; text-decoration: none'>github.com/seb821</a></h6></footer>\r\n";
    htmlMessage += "</body>\r\n</html>\r\n";
	DEBUG_PRINTLN("END of home webpage");DEBUG_PRINT("Free mem: ");DEBUG_PRINTLN(ESP.getFreeHeap());
    httpserver.send(200, "text/html", htmlMessage);
  }); // server.on("/"...



  httpserver.on("/livedata",[](){       // 
	DEBUG_PRINTLN("Html page requested: /live-data");
	
    String htmlMessage = "<!DOCTYPE html>\r\n<html>\r\n";

    // Head
    htmlMessage += "<head><title>" + String(MQTT_RFLINK_CLIENT_NAME) + "</title>\r\n";
    htmlMessage += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>\r\n";
	htmlMessage += "<link rel=\"stylesheet\" type=\"text/css\" href=\"esp.css\">\r\n";
	htmlMessage += String(cssDatasheet);	// CSS
    htmlMessage += "</head>\r\n";

    // Body
    htmlMessage += "<body class='bodymenu'>\r\n";

    // Header + menu
    htmlMessage += "<header class='headermenu'>";
    htmlMessage += "<h1>" + String(MQTT_RFLINK_CLIENT_NAME) + "</h1>\r\n";
    htmlMessage += "<div class='menubar'><a class='menu' href='.'>&#8962;<span class='showmenulabel'>Main</span></a>\r\n";
    htmlMessage += "<a class='menu active' href='/livedata'>&#10740;<span class='showmenulabel'>Live Data</span></a>\r\n";
    htmlMessage += "<a id='resetmega' class='menu' href='/reset-mega' onclick = \"fetch('/reset-mega');return false;\">&#128204;<span class='showmenulabel'>Reset MEGA</span></a>\r\n";
    htmlMessage += "<a class='menu' id='reboot' href='/reboot'>&#128268;<span class='showmenulabel'>Reboot ESP</span></a>\r\n";
    htmlMessage += (!MQTT_DEBUG)? "<a class='menu' href='/enable-debug'>&#128172;<span class='showmenulabel'>Enable MQTT debug</span></a>\r\n":"<a class='menu' href='/disable-debug'>&#128172;<span class='showmenulabel'>Disable MQTT debug</span></a>\r\n";
    htmlMessage += "<a class='menu' href='/infos'>&#128295;<span class='showmenulabel'>System Info</span></a></div>\r\n";
    htmlMessage += "</header>\r\n";
    
    // Live data
    htmlMessage += "<h3>RFLink Live Data</h3>\r\n";

    htmlMessage += "<input type=\"button\" value =\"Pause\" onclick=\"stopUpdate();\" />";                                                                // Pause
    htmlMessage += "<input type=\"button\" value =\"Restart\" onclick=\"restartUpdate();\" />";                                                           // Restart
    htmlMessage += "<input type=\"button\" value =\"Refresh\" onclick=\"window.location.reload(true);\" />\r\n";                                          // Refresh                                                                                   // OK
    htmlMessage += "<input type=\"text\" id=\"mySearch\" onkeyup=\"filterLines()\" placeholder=\"Search for...\" title=\"Type in a name\"><br />\r\n";    // Search
    
    htmlMessage += "<table id=\"liveData\" class='multirow multirow-left';>\r\n";                                                                         // Table of x lines
    htmlMessage += "<tr class=\"header\"><th style='text-align:left;'> <a onclick='sortTable(0)'>Time</a></th><th style='text-align:left;'> <a onclick='sortTable(1)'>Raw Data</a> </th><th style='text-align:left;'> <a onclick='sortTable(2)'>MQTT Topic</a> </th><th style='text-align:left;'> <a onclick='sortTable(3)'>MQTT JSON</a> </th></tr>\r\n";
	htmlMessage += "<tr id=\"data0" "\"><td></td><td></td><td></td><td></td></tr>\r\n";
    htmlMessage += "</table>\r\n";
    
    htmlMessage += "<script>\r\n";                                                       // Script to filter lines
    htmlMessage += "function filterLines() {\r\n";
    htmlMessage += "  var input, filter, table, tr, td, i;\r\n";
    htmlMessage += "  input = document.getElementById(\"mySearch\");\r\n";
    htmlMessage += "  filter = input.value.toUpperCase();\r\n";
    htmlMessage += "  table = document.getElementById(\"liveData\");\r\n";
    htmlMessage += "  tr = table.getElementsByTagName(\"tr\");\r\n";
    htmlMessage += "  for (i = 0; i < tr.length; i++) {\r\n";
    htmlMessage += "    td = tr[i].getElementsByTagName(\"td\")[1];\r\n";
    htmlMessage += "    if (td) {\r\n";
    htmlMessage += "      if (td.innerHTML.toUpperCase().indexOf(filter) > -1) {\r\n";
    htmlMessage += "        tr[i].style.display = \"\";\r\n";
    htmlMessage += "      } else {\r\n";
    htmlMessage += "        tr[i].style.display = \"none\";\r\n";
    htmlMessage += "      }\r\n";
    htmlMessage += "    }       \r\n";
    htmlMessage += "  }\r\n";
    htmlMessage += "}\r\n";
    htmlMessage += "</script>\r\n";
    htmlMessage += "<script>\r\n";                                                        // Script to update data and move to next line
    htmlMessage += "var x = setInterval(function() {loadData(\"data.txt\",updateData)}, 250);\r\n";       // update every 250 ms
    htmlMessage += "function loadData(url, callback){\r\n";
    htmlMessage += "var xhttp = new XMLHttpRequest();\r\n";
    htmlMessage += "xhttp.onreadystatechange = function(){\r\n";
    htmlMessage += " if(this.readyState == 4 && this.status == 200){\r\n";
    htmlMessage += " callback.apply(xhttp);\r\n";
    htmlMessage += " }\r\n";
    htmlMessage += "};\r\n";
    htmlMessage += "xhttp.open(\"GET\", url, true);\r\n";
    htmlMessage += "xhttp.send();\r\n";
    htmlMessage += "}\r\n";
    htmlMessage += "var memorized_data;\r\n";
    htmlMessage += "function roll() {\r\n"
      "var table = document.getElementById('liveData');\r\n"
      "var rows = table.rows;\r\n"
      "var firstRow = rows[1];\r\n"
      "var clone = firstRow.cloneNode(true);\r\n"
	  "var target = rows[1];\r\n"
	  "var newElement = clone;\r\n"
	  "target.parentNode.insertBefore(newElement, target.nextSibling );\r\n"
	  "}\r\n";
    htmlMessage += "function updateData(){\r\n";
    htmlMessage += "if (memorized_data != this.responseText) {\r\n";
	htmlMessage += "roll();";
    htmlMessage += "var date = new Date;\r\n";
    htmlMessage += "h = date.getHours(); if(h<10) {h = '0'+h;}; m = date.getMinutes(); if(m<10) {m = '0'+m;}; s = date.getSeconds(); if(s<10) {s = '0'+s;}\r\n";
    htmlMessage += "document.getElementById('data0').innerHTML = '<td>' + h + ':' + m + ':' + s + '</td>' + this.responseText;\r\n";
    htmlMessage += "memorized_data = this.responseText;\r\n"; // memorize new data
    htmlMessage += "filterLines();\r\n";                      // apply filter from mySearch input
    htmlMessage += "}\r\n";
    htmlMessage += "}\r\n";
    htmlMessage += "function stopUpdate(){\r\n";
    htmlMessage += " clearInterval(x);\r\n";
    htmlMessage += "}\r\n";
    htmlMessage += "function restartUpdate(){\r\n";
    htmlMessage += " x = setInterval(function() {loadData(\"data.txt\",updateData)}, 250);\r\n";       // update every 250 ms
    htmlMessage += "}\r\n";
    htmlMessage += "</script>\r\n";
	htmlMessage += ""
		"<script>\r\n"
		"function sortTable(column) {\r\n"
		"  var table, rows, switching, i, x, y, shouldSwitch;\r\n"
		"  table = document.getElementById(\"liveData\");\r\n"
		"  switching = true;\r\n"
		"  while (switching) {\r\n"
		"	switching = false;\r\n"
		"	rows = table.rows;\r\n"
		"	for (i = 1; i < (rows.length - 1); i++) {\r\n"
		"	  shouldSwitch = false;\r\n"
		"	  x = rows[i].getElementsByTagName(\"TD\")[column];\r\n"
		"	  y = rows[i + 1].getElementsByTagName(\"TD\")[column];\r\n"
		"	  if (x.innerHTML.toLowerCase() < y.innerHTML.toLowerCase()) {\r\n"
		"		shouldSwitch = true;\r\n"
		"		break;\r\n"
		"	  }\r\n"
		"	}\r\n"
		"	if (shouldSwitch) {\r\n"
		"	  rows[i].parentNode.insertBefore(rows[i + 1], rows[i]);\r\n"
		"	  switching = true;\r\n"
		"	}\r\n"
		"  }\r\n"
		"}\r\n"
		"</script>\r\n";
    htmlMessage += "<span style=\"font-size: 0.9em\">Running for " + time_string_exp(millis()) + " </span>\r\n";
    htmlMessage += "<input type=\"button\" value =\"Refresh\" onclick=\"window.location.reload(true);\" />\r\n";
    htmlMessage += "<footer><br><h6>Powered by <a href='https://github.com/seb821' style='font-size: 15px; text-decoration: none'>github.com/seb821</a></h6></footer>\r\n";
    htmlMessage += "</body>\r\n</html>\r\n";
    httpserver.send(200, "text/html", htmlMessage);
	
  }); // livedata
  
  httpserver.on("/reboot",[](){                                	// Reboot ESP
	DEBUG_PRINTLN("Html page requested: /reboot");
	
    DEBUG_PRINTLN("Rebooting ESP...");
    httpserver.send(200, "text/html", "Rebooting ESP... <a href='/'>Home</a>");
	  delay(500);
    ESP.restart();
    //ESP.reset();
  });

  httpserver.on("/reset-mega",[](){                             // Reset MEGA
	DEBUG_PRINTLN("Html page requested: /reset-mega");
	
    DEBUG_PRINTLN("Resetting Mega...");
    httpserver.send(200, "text/html", "Resetting Mega... <a href='/'>Home</a>");
    #if defined(MQTT_MEGA_RESET_TOPIC)
      MQTTClient.publish(MQTT_MEGA_RESET_TOPIC,"1");
      delay(1000);
      MQTTClient.publish(MQTT_MEGA_RESET_TOPIC,"0");
    #endif
    delay(200);
    pinMode(MEGA_RESET_PIN, OUTPUT);
    delay(200);
    digitalWrite(MEGA_RESET_PIN,false);                         // Change the state of pin to ground
    delay(1000); 
    digitalWrite(MEGA_RESET_PIN,true);                          // Change the state of pin to VCC
    delay(50);

	});

  httpserver.on("/send",[](){                                   // Handle inputs from web interface
	DEBUG_PRINTLN("Html page requested: /send");
	
    if (httpserver.args() > 0 ) {
        for ( uint8_t i = 0; i < httpserver.args(); i++ ) {
          
          if (httpserver.argName(i) == "command") {             // Send command to RFLInk from web interface, check it comes from command input in html form
             String text_command = httpserver.arg(i);           // Get command send
             byte buf[text_command.length() + 1];               // Temp char for conversion
             text_command.getBytes(buf, sizeof(buf));
             rflinkSerialTX.write(buf, sizeof(buf));            // Write command to RFLink serial
             //rflinkSerialTX.print(httpserver.arg(i));
             rflinkSerialTX.print(F("\r\n"));
             DEBUG_PRINTLN();
             DEBUG_PRINTLN(F("=== Web command ==="));
             DEBUG_PRINT(F("message = "));
             DEBUG_WRITE(buf, sizeof(buf));
             DEBUG_PRINT(httpserver.arg(i));
             DEBUG_PRINTLN(F("\r\n"));
          }
          
       }
    }
    httpserver.sendHeader("Location","/");       	// Add a header to respond with a new location for the browser to go to the home page again
    httpserver.send(303);                       	// Send it back to the browser with an HTTP status 303 to redirect
    
  });
  
  httpserver.on("/enable-debug",[](){
    DEBUG_PRINTLN("Enabling MQTT debug...");
    MQTT_DEBUG = 1;
    httpserver.sendHeader("Location","/");      	// Add a header to respond with a new location for the browser to go to the home page again
    httpserver.send(303);                     		// Send it back to the browser with an HTTP status 303 to redirect
  });

  httpserver.on("/disable-debug",[](){
    DEBUG_PRINTLN("Disabling MQTT debug...");
    MQTT_DEBUG = 0;
	MQTTClient.publish(MQTT_DEBUG_TOPIC,"{\"DATA\":\" \",\"ID\":\" \",\"NAME\":\" \",\"TOPIC\":\" \",\"JSON\":\" \"}");
    httpserver.sendHeader("Location","/");     		// Add a header to respond with a new location for the browser to go to the home page again
    httpserver.send(303);                       	// Send it back to the browser with an HTTP status 303 to redirect
  });
  
  httpserver.on("/data.txt", [](){            		// Used to deliver raw data received (BUFFER) and mqtt data published (MQTT_TOPIC and JSON)           
    httpserver.send(200, "text/html", "<td>" + String(BUFFER) + "</td><td>" + String(MQTT_TOPIC) + "</td><td>" + String(JSON) + "</td>\r\n");
  });

  httpserver.on("/wifi-scan", [](){
	int numberOfNetworks = WiFi.scanNetworks();
	String htmlMessage = "";
	for(int i =0; i<numberOfNetworks; i++){
		htmlMessage += String(WiFi.SSID(i)) + ":  " + String(WiFi.RSSI(i)) +" dBm<br>\r\n";
   
	}
    httpserver.send(200, "text/html", htmlMessage);
  });

  httpserver.on("/infos", [](){
	DEBUG_PRINTLN("Html page requested: /infos");
	
	String htmlMessage = "<!DOCTYPE html>\r\n<html>\r\n";

    // Head
    htmlMessage += "<head><title>" + String(MQTT_RFLINK_CLIENT_NAME) + " - Infos</title>\r\n";
    htmlMessage += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>\r\n";
    htmlMessage += "<meta http-equiv='Content-Type' content='text/html; charset=UTF-8' />\r\n";
	htmlMessage += "<link rel=\"stylesheet\" type=\"text/css\" href=\"esp.css\">\r\n";
	htmlMessage += String(cssDatasheet);	// CSS
    htmlMessage += "</head>\r\n";

    // Body
   htmlMessage += "<body class='bodymenu'>\r\n";
   
	// Header + menu
	htmlMessage += "<header class='headermenu'>";
	htmlMessage += "<h1>" + String(MQTT_RFLINK_CLIENT_NAME) + "</h1>\r\n";
	htmlMessage += "<div class='menubar'><a class='menu' href='.'>&#8962;<span class='showmenulabel'>Main</span></a>\r\n";
	htmlMessage += "<a class='menu' href='/livedata'>&#10740;<span class='showmenulabel'>Live Data</span></a>\r\n";
	htmlMessage += "<a id='resetmega' class='menu' href='/reset-mega' onclick = \"fetch('/reset-mega');return false;\">&#128204;<span class='showmenulabel'>Reset MEGA</span></a>\r\n";
	htmlMessage += "<a class='menu' id='reboot' href='/reboot'>&#128268;<span class='showmenulabel'>Reboot ESP</span></a>\r\n";
	htmlMessage += (!MQTT_DEBUG)? "<a class='menu' href='/enable-debug'>&#128172;<span class='showmenulabel'>Enable MQTT debug</span></a>\r\n":"<a class='menu' href='/disable-debug'>&#128172;<span class='showmenulabel'>Disable MQTT debug</span></a>\r\n";
	htmlMessage += "<a class='menu active' href='/infos'>&#128295;<span class='showmenulabel'>System Info</span></a></div>\r\n";
	htmlMessage += "</header>\r\n";	
   
    // System Info
    htmlMessage += "<h3>System Info</h3>\r\n";
    htmlMessage += "<table class='normal'>\r\n";
    htmlMessage += "<tr><td style='min-width:150px;'>Version</td><td style='width:80%;'>" + String(VERSION) + "</td></tr>\r\n";
    htmlMessage += "<tr><td>Uptime</td><td>" + String(time_string_exp(millis())) + "</td></tr>\r\n";
    htmlMessage += "<tr><td>WiFi network</td><td>" + String(WiFi.SSID()) + " (" + WiFi.BSSIDstr() + ")</td></tr>\r\n";
    htmlMessage += "<tr><td>WiFi RSSI</td><td>" + String(WiFi.RSSI()) + " dB</td></tr>\r\n";
    htmlMessage += "<tr><td>IP address (MAC)</td><td>" + WiFi.localIP().toString() +" (" + String(WiFi.macAddress()) +")</td></tr>\r\n";
	htmlMessage += "<tr><td>ESP pin to reset MEGA</td><td>GPIO " + String(MEGA_RESET_PIN);
	if (resetMegaInterval > 0) {
		htmlMessage += " - auto reset after " + String( int(float(resetMegaInterval) *0.001 /60)) + " min if no data received - last data " + time_string_exp(now - lastReceived) + " ago";// - " + resetMegaCounter + " resets since ESP reboot";
	}
	htmlMessage += "</td></tr>";
    htmlMessage += "<tr><td>MQTT server and port</td><td>" + String(MQTT_SERVER) + ":" + String(MQTT_PORT)+"</td></tr>\r\n";
    htmlMessage += "<tr><td>MQTT debug</td><td>";
    (MQTT_DEBUG)? htmlMessage += "<span style=\"font-weight:bold\">enabled</span>" : htmlMessage += "disabled";
    htmlMessage += "</td></tr>\r\n";
    htmlMessage += "<tr><td>MQTT connection state <a class='button help' href='https://pubsubclient.knolleary.net/api.html#state' target='_blank'>&#10068;</a></td><td>" + String(MQTTClient.state()) + "</td></tr>\r\n";
    htmlMessage += "<tr><td>MQTT topics</td><td><table class='condensed'>\r\n";
    htmlMessage += "<tr><td>publish (json)</td><td> " + String(MQTT_PUBLISH_TOPIC) + "/Protocol_Name-ID</td></tr>\r\n";
    htmlMessage += "<tr><td>commands to rflink</td><td> " + String(MQTT_RFLINK_ORDER_TOPIC) + "</td></tr>\r\n";
    htmlMessage += "<tr><td>last will ( " + String(MQTT_WILL_ONLINE) + " / " + String(MQTT_WILL_OFFLINE) + " )</td><td> " + String(MQTT_WILL_TOPIC) + "</td></tr>\r\n";
    htmlMessage += "<tr><td>uptime (min, every " + String( int( float(uptimeInterval) *0.001 / 60) ) + ")</td><td> " + String(MQTT_UPTIME_TOPIC) + "</td></tr>\r\n";
    htmlMessage += "<tr><td>debug (data from rflink)</td><td> " + String(MQTT_DEBUG_TOPIC) + "</td></tr>\r\n";
  if (resetMegaInterval > 0) {
    htmlMessage += "<tr><td>mega reset information</td><td> " + String(MQTT_MEGA_RESET_TOPIC) + "</td></tr>\r\n";
  }
    htmlMessage += "</table></td></tr>\r\n";
    for (int i = 0; i < (sizeof(USER_SPECIFIC_IDs) / sizeof(USER_SPECIFIC_IDs[0])); i++){      // User specific IDs defined in USER_SPECIFIC_IDs
          htmlMessage += "<tr><td>User specific</td><td>ID for protocol " + String(USER_SPECIFIC_IDs[i].name) + " is forced to " + String(USER_SPECIFIC_IDs[i].id_applied) + "; applies to ID: " + String(USER_SPECIFIC_IDs[i].id_filtered) +"</td></tr>\r\n";
    }

	
    htmlMessage += "<tr><td>Free Mem</td><td>" + String (ESP.getFreeHeap()) + "</td></tr>\r\n";
    register uint32_t *sp asm("a1");
    htmlMessage += "<tr><td>Free Stack</td><td>" +  String(4 * (sp - g_pcont->stack) ) + "</td></tr>\r\n";
    htmlMessage += "<tr><td>Heap Max Free Block</td><td>" + String(ESP.getMaxFreeBlockSize()) + "</td></tr>\r\n"; 
    htmlMessage += "<tr><td>Heap Fragmentation</td><td>" + String (ESP.getHeapFragmentation()) + "%</td></tr>\r\n";
	
	htmlMessage += "</table><br />\r\n";
	
	htmlMessage += "<a href='/wifi-scan' class='button link'>Scan Wifi Networks</a> <a href='/update' class='button link'>Load ESP firmware</a><br><br>\r\n";
	
	htmlMessage += "</body></html>";
	
    httpserver.send(200, "text/html", htmlMessage);
  });
  
  httpserver.on("/configuration", [](){                // Used to change USER_IDs configuration in EEPROM
	DEBUG_PRINTLN("Html page requested: /configuration");
	
	// URL arguments
	int page = 0;
	String arg_page = httpserver.arg("page");
	if (arg_page.length() > 0) {
		page = max((int) arg_page.toInt(),0);
	};
	String arg_line = httpserver.arg("line");
	String arg_id = httpserver.arg("id");
	String arg_id_applied = httpserver.arg("id_applied");
	String arg_description = httpserver.arg("description");
	String arg_publish_interval = httpserver.arg("publish_interval");
	if (arg_line.length() > 0) {
	  int i = arg_line.toInt();
	  if (i <= USER_ID_NUMBER) {
		arg_id.toCharArray(eepromConfig.user_id[i].id,MAX_ID_LEN);
		arg_id_applied.toCharArray(eepromConfig.user_id[i].id_applied,MAX_ID_LEN);
		arg_description.toCharArray(eepromConfig.user_id[i].description,MAX_DATA_LEN+1);
		eepromConfig.user_id[i].publish_interval = arg_publish_interval.toInt();
	  }
	}
		
    String htmlMessage = "<!DOCTYPE html>\r\n<html>\r\n";

    // Head
    htmlMessage += "<head>\r\n<title>" + String(MQTT_RFLINK_CLIENT_NAME) + " - Configuration</title>\r\n";
    htmlMessage += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>\r\n";
    htmlMessage += "<meta http-equiv='Content-Type' content='text/html; charset=UTF-8' />\r\n";
	htmlMessage += "<link rel=\"stylesheet\" type=\"text/css\" href=\"esp.css\">\r\n";
	htmlMessage += String(cssDatasheet);	// CSS
    htmlMessage += "</head>\r\n";

    // Body
	htmlMessage += "<body class='bodymenu'>\r\n";
	htmlMessage += "<a href='/' class='button link'>Home</a>\r\n";
	htmlMessage += "<br><br>\r\n";
   
   htmlMessage += "<h3>User IDs online configuration\r\n";
   if (USER_ID_NUMBER > 0) {                      // show list of user IDs
	if (USER_ID_NUMBER > 10) {
		for (int i = 0; i <= ((int) (USER_ID_NUMBER-1)/10); i++){
		   htmlMessage += "<a href='/configuration?page=" + String(i) + "' class='button link'>" + String(i*10+1) + " - " + String(min(USER_ID_NUMBER,i*10+10)) + "</a>\r\n";
		}
	}
	htmlMessage += "</h3>\r\n";
	htmlMessage += "<br>\r\n";
   


      htmlMessage += "<table class='multirow'><tr><th>#</th><th>ID</th><th>ID applied</th><th>Description</th><th>Interval (ms)  </th><th></th></tr>\r\n";
      for (int i = 0+(10*page); i < min(USER_ID_NUMBER,10*(page+1)); i++){
        htmlMessage += "<tr><form method='get' action='/configuration'>";
		htmlMessage += "<td><input type='hidden' name='line' value=" + String(i) + "><input type='hidden' name='page' value=" + page + ">" + String(i+1) + "</td>";
        htmlMessage += "<td><input type='text' name='id' size='" + String(MAX_ID_LEN) + "' value='" + String(eepromConfig.user_id[i].id) + "'></td>";
        htmlMessage += "<td><input type='text' name='id_applied' size='" + String(MAX_ID_LEN) + "' value='" + String(eepromConfig.user_id[i].id_applied) + "'></td>";
        htmlMessage += "<td style='text-align: left;'><input type='text' name='description' size='" + String(MAX_DATA_LEN) + "' value='" + String(eepromConfig.user_id[i].description) + "'></td>";
        htmlMessage += "<td><input type='number' style='width: 8em' name='publish_interval' step='1000' value='" + String(eepromConfig.user_id[i].publish_interval) + "'></td>";
        htmlMessage += "<td><input type='submit' value='Apply'></td></form></tr>\r\n";
      }
      htmlMessage += "</table><br>\r\n";
      htmlMessage += "<form method='get' action='save-eeprom'><input type='submit' value='Save to EEPROM'> => Save above configuration to EEPROM (lost on reboot otherwise)</form>";  
      htmlMessage += "<br><br>Code to update USER_IDs[] in config.h with current eeprom configuration:<br>\r\n";
      htmlMessage += "<xmp>\r\n";

	  
      for (int i = 0; i < (USER_ID_NUMBER); i++){
        String str_description = String(eepromConfig.user_id[i].description);     
        htmlMessage += "  {\"" + String(eepromConfig.user_id[i].id) + "\",\"" + String(eepromConfig.user_id[i].id_applied) + "\"," + String(eepromConfig.user_id[i].publish_interval) + ",\"" + str_description + "\"}, //" + (i+1) + "\r\n";
      }
      htmlMessage += "</xmp><br>\r\n";

      htmlMessage += "<form method='get' action='erase-eeprom'><input type='submit' value='Erase EEPROM'> => Restore configuration from firmware</form>\r\n";	
    }
	htmlMessage += "<br><br><span style=\"font-size: 0.9em\">Running for " + time_string_exp(millis()) + " - Free Mem " + String (ESP.getFreeHeap()) + "</span>\r\n";
	htmlMessage += "</body>\r\n</html>";
    httpserver.send(200, "text/html", htmlMessage);
  });

  httpserver.on("/save-eeprom",[](){                                  // Saving EEPROM + reboot
    DEBUG_PRINTLN("Html page requested: /save-eeprom");
	DEBUG_PRINTLN("Saving EEPROM");
    saveEEPROM();
    DEBUG_PRINTLN("Rebooting device...");
    httpserver.send(200, "text/html", "EEPROM saved, rebooting... <a href='/configuration'>Configuration</a>");
    delay(500);
    ESP.restart();
    //ESP.reset();
  });

  httpserver.on("/erase-eeprom",[](){                                  // Erasing EEPROM + reboot
	DEBUG_PRINTLN("Html page requested: /erase-eeprom");
    DEBUG_PRINTLN("Erasing EEPROM");
	EEPROM.begin(eepromLength);
	for (int i = eepromAddress; i < (eepromAddress + eepromLength); i++) {
	EEPROM.write(i, 0);
	EEPROM.end();
	}
    DEBUG_PRINTLN("Rebooting device...");
    httpserver.send(200, "text/html", "EEPROM erased, rebooting... <a href='/configuration'>Configuration</a>");
    delay(500);
    ESP.restart();
    //ESP.reset();
	
  });
  
  httpserver.onNotFound([](){
    httpserver.send(404, "text/plain", "404: Not found");      	// Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
  });

};

/**
 * MQTT publish without json
 */
 
void pubFlatMqtt(char* topic,char* json)          	// Unused but could be usefull
{
        DynamicJsonBuffer jsonBuffer(BUFFER_SIZE);
        JsonObject& root = jsonBuffer.parseObject(json);
        char myTopic[40];
        // Test if parsing succeeds.
        if (!root.success()) {
                DEBUG_PRINTLN("parseObject() failed");
                return;
        }
        for (auto kv : root) {
                DEBUG_PRINT(kv.key);
                DEBUG_PRINT(F(" => "));
                DEBUG_PRINTLN(kv.value.as<char*>());
                strcpy(myTopic,topic);
                strcat(myTopic,"/");
                strcat(myTopic,kv.key);
                MQTTClient.publish(myTopic,kv.value.as<char*>());
        }
}

/**
 * Time functions
 */

long uptime_min()
{
 long days=0;
 long hours=0;
 long mins=0;
 long secs=0;
 secs = millis()/1000; 		//convect milliseconds to seconds
 mins=secs/60; 				//convert seconds to minutes
 hours=mins/60; 			//convert minutes to hours
 days=hours/24; 			//convert hours to days
 return mins;
}

/*
String uptime_string_exp()
{
  String result;
  char strUpTime[40];
  int minutes = int(float(millis()) *0.001 / 60);
  int days = minutes / 1440;
  minutes = minutes % 1440;
  int hrs = minutes / 60;
  minutes = minutes % 60;
  sprintf_P(strUpTime, PSTR("%d days %d hours %d minutes"), days, hrs, minutes);
  result = strUpTime;
  return result;
} */

String time_string_exp(long time)
{
  String result;
  char strUpTime[40];
  int minutes = int(float(time) *0.001 / 60);
  int days = minutes / 1440;
  minutes = minutes % 1440;
  int hrs = minutes / 60;
  //int hrs = ( minutes / 60 ) + ( days * 24 );
  minutes = minutes % 60;
  if (days > 0) {
    sprintf_P(strUpTime, PSTR("%d d %d h %d min"), days, hrs, minutes);
  } else if (hrs > 0) {
	  sprintf_P(strUpTime, PSTR("%d h %d min"), hrs, minutes);
  } else {
	  sprintf_P(strUpTime, PSTR("%d min"), minutes);
  }
  result = strUpTime;
  return result;
} 


/*********************************************************************************
 * Classic arduino bootstrap
/*********************************************************************************/

void setup() {
   

        // Open serial communications and wait for port to open:
        
        //DEBUG_BEGIN(115200);
        debugSerialTX.begin(57600);        // debug serial : same speed as RFLInk
			while (!debugSerialTX) {
					; // wait for serial port to connect. Needed for native USB port only
			}
        delay(1000);
        DEBUG_PRINTLN();
        DEBUG_PRINTLN(F("Init debug serial done"));

        DEBUG_PRINTLN(F("Starting..."));

        // Set the data rate for the RFLink serial
        rflinkSerialTX.begin(57600);
        DEBUG_PRINTLN();
        DEBUG_PRINTLN(F("Init rflink serial done"));

        // Setup WIFI
        setup_wifi();

        // Setup MQTT
        MqttConnect();

        // Setup OTA
        SetupOTA();
        ArduinoOTA.begin();
        DEBUG_PRINT("Ready for OTA on ");
        DEBUG_PRINT("IP address:\t");
        DEBUG_PRINTLN(WiFi.localIP());

        // Setup HTTP Server
        ConfigHTTPserver();
        httpUpdater.setup(&httpserver);                                 // Firmware webupdate
        //DEBUG_PRINTF("HTTPUpdateServer ready! Open http://%s.local/update in your browser\n", MQTT_RFLINK_CLIENT_NAME);
        DEBUG_PRINTLN("HTTPUpdateServer ready");
        httpserver.begin();                                             // Actually start the HTTP server
        DEBUG_PRINTLN("HTTP server started"); 
        
   if (USER_ID_NUMBER > 0) {

        // Load USER_IDs into EEPROM if version changed
          loadEEPROM();
          DEBUG_PRINT("Size of eepromConfig: ");DEBUG_PRINTLN(sizeof(eepromConfig));
          DEBUG_PRINT("Size of USER_IDs: ");DEBUG_PRINTLN(sizeof(USER_IDs));
          DEBUG_PRINT("Size of matrix: ");DEBUG_PRINTLN(sizeof(matrix));
		
        // Get values from EEPROM into matrix
        //for (int i =0; i < USER_ID_NUMBER; i++) {
            //strcpy(matrix[i].id,eepromConfig.user_id[i].id);
			//strcpy(matrix[i].id_applied,eepromConfig.user_id[i].id_applied);
            //matrix[i].publish_interval = eepromConfig.user_id[i].publish_interval;
            //strcpy(matrix[i].description,eepromConfig.user_id[i].description);
        //}
    }

        rflinkSerialTX.println();
        delay(1000);
        //rflinkSerialTX.println(F("10;status;"));                  // Ask status to RFLink
        rflinkSerialTX.println(F("10;ping;"));                        // Do a PING on startup
        delay(200);
        rflinkSerialTX.println(F("10;version;"));                     // Ask version to RFLink
        
} // setup

/*********************************************************************************
 * Main loop
/*********************************************************************************/

void loop() {
	bool DataReady=false;
	// handle lost of connection : retry after 1s on each loop
	if (!MQTTClient.connected()) {
		DEBUG_PRINTLN(F("Not connected, retrying in 1s"));
		delay(1000);
		MqttConnect();
	} else {
		// if something arrives from rflink
		if(rflinkSerialRX.available()) {
			char rc;
			// bufferize serial message
			while(rflinkSerialRX.available() && CPT < BUFFER_SIZE) {
				rc = rflinkSerialRX.read();
				if (isAscii(rc)) { // ensure char is ascii, this is to stop bad chars being sent https://www.arduino.cc/en/Tutorial/CharacterAnalysis
					BUFFER[CPT] = rc;
					CPT++;
					if (BUFFER[CPT-1] == '\n') {
						  DataReady = true;
						  BUFFER[CPT]='\0';
						  CPT=0;
					}
				}
			}
			if (CPT > BUFFER_SIZE ) CPT=0;
		}

		// parse what we just read
		if (DataReady) {
			
			// clean variables
			strcpy(MQTT_ID,""); strcpy(MQTT_NAME,"");strcpy(MQTT_TOPIC,"");strcpy(JSON,"");strcpy(JSON_DEBUG,"");

			// read data
			readRfLinkPacket(BUFFER);

			// Store last received data time if MQTT_ID is valid
			if ( (strcmp(MQTT_ID,"") != 0) && (strcmp(MQTT_ID,"0\0") != 0) ) lastReceived = millis();
			
			// If USER_SPECIFIC_IDs is used
			for (int i = 0; i < (sizeof(USER_SPECIFIC_IDs) / sizeof(USER_SPECIFIC_IDs[0])); i++){
			  if (strcmp(MQTT_NAME,USER_SPECIFIC_IDs[i].name) == 0){    			// check if protocol / name matches
				if (strcmp(USER_SPECIFIC_IDs[i].id_filtered,"ALL") == 0) {   		// applies new ID to all IDs for specific name/protocol
					strcpy( MQTT_ID , USER_SPECIFIC_IDs[i].id_applied);
				} else if (strcmp(USER_SPECIFIC_IDs[i].id_filtered,MQTT_ID) == 0) {
					strcpy( MQTT_ID , USER_SPECIFIC_IDs[i].id_applied);       		// applies new ID to specific ID for specific name/protocol
				}
			  }
			}
			
			// construct topic name to publish to
			buildMqttTopic();						
			
			// report message for debugging
			printToSerial();
			
			// MQTT debug mode with json, full data (published in several times to avoid changing MQTT_MAX_PACKET_SIZE in PubSubClient.h)
			if (MQTT_DEBUG) {										
				strcpy(BUFFER_DEBUG,"{");
				strcat(BUFFER_DEBUG,"\"DATA\":\"");strncat(BUFFER_DEBUG,BUFFER,strlen(BUFFER)-2);strcat(BUFFER_DEBUG,"\"");
				strcat(BUFFER_DEBUG,",\"ID\":\"");strcat(BUFFER_DEBUG,MQTT_ID);strcat(BUFFER_DEBUG,"\"");
				strcat(BUFFER_DEBUG,"}");
				MQTTClient.publish(MQTT_DEBUG_TOPIC,BUFFER_DEBUG);
				strcpy(BUFFER_DEBUG,"{");
				strcat(BUFFER_DEBUG,"\"ID\":\"");strcat(BUFFER_DEBUG,MQTT_ID);strcat(BUFFER_DEBUG,"\"");
				strcat(BUFFER_DEBUG,",\"NAME\":\"");strcat(BUFFER_DEBUG,MQTT_NAME);strcat(BUFFER_DEBUG,"\"");
				strcat(BUFFER_DEBUG,",\"TOPIC\":\"");strcat(BUFFER_DEBUG,MQTT_TOPIC);strcat(BUFFER_DEBUG,"\"");
				strcat(BUFFER_DEBUG,"}");
				MQTTClient.publish(MQTT_DEBUG_TOPIC,BUFFER_DEBUG);
				strcpy(BUFFER_DEBUG,"{");
				strcpy(JSON_DEBUG,JSON);
				for (char* p = JSON_DEBUG; p = strchr(p, '\"'); ++p) {*p = '\'';} // Remove quotes
				strcat(BUFFER_DEBUG,"\"JSON\":\"");strcat(BUFFER_DEBUG,JSON_DEBUG);strcat(BUFFER_DEBUG,"\"");
				strcat(BUFFER_DEBUG,",\"ID\":\"");strcat(BUFFER_DEBUG,MQTT_ID);strcat(BUFFER_DEBUG,"\"");
				strcat(BUFFER_DEBUG,"}");
				MQTTClient.publish(MQTT_DEBUG_TOPIC,BUFFER_DEBUG);
				}
			
			// MQTT debug mode, raw data only	
			//if (MQTT_DEBUG) MQTTClient.publish(MQTT_DEBUG_TOPIC,BUFFER);      

			
			// XXX publish to MQTT server only if ID is authorized
			
			// ID filtering with USER_IDs 
			if (USER_ID_NUMBER > 0) {                         
				for (int i = 0; i < (USER_ID_NUMBER); i++){
					
					DEBUG_PRINT("ID = ");DEBUG_PRINT(eepromConfig.user_id[i].id);
					DEBUG_PRINT(" ; ID_applied = ");DEBUG_PRINT(eepromConfig.user_id[i].id_applied);
					DEBUG_PRINT(" ; publish_interval = ");DEBUG_PRINT(eepromConfig.user_id[i].publish_interval);
					DEBUG_PRINT(" ; decription = ");DEBUG_PRINT(eepromConfig.user_id[i].description);
					DEBUG_PRINT(" ; json = ");DEBUG_PRINT(matrix[i].json);
					//DEBUG_PRINT(" ; json_checksum = ");DEBUG_PRINT(matrix[i].json_checksum);
					DEBUG_PRINT(" ; last_published = ");DEBUG_PRINT(matrix[i].last_published);
					DEBUG_PRINT(" ; last_received = ");DEBUG_PRINTLN(matrix[i].last_received);

					if (strcmp(MQTT_ID,eepromConfig.user_id[i].id) == 0) {                          // check ID is authorized
					  DEBUG_PRINT("Authorized ID ");DEBUG_PRINT(MQTT_ID);
					  matrix[i].last_received = millis();                                         	// memorize received time
					  if (strcmp(eepromConfig.user_id[i].id_applied,"") != 0 ) {					// apply different ID if available
						 strcpy(MQTT_ID,eepromConfig.user_id[i].id_applied);
						 buildMqttTopic();															// rebuild MQTT_TOPIC
						}
					  if (strncmp(matrix[i].json,JSON,sizeof(matrix[i].json)-2) != 0) {                                     	// check if json data has changed
					  //if (matrix[i].json_checksum != json_checksum) {                                   // check if json_checksum has changed
						DEBUG_PRINT(" => data changed => published on ");DEBUG_PRINTLN(MQTT_TOPIC);
						MQTTClient.publish(MQTT_TOPIC,JSON);                           			  	// if changed, publish on MQTT server  
						strncpy(matrix[i].json,JSON,sizeof(matrix[i].json)-2);                       	// memorize new json value
						//matrix[i].json_checksum = json_checksum;                                 	// memorize new json_checksum
						matrix[i].last_published = millis();                                      	// memorize published time           
					  } else {                                                                    	// no data change
						now = millis();
						if ( (now - matrix[i].last_published) > (eepromConfig.user_id[i].publish_interval) ) {   	// check if it exceeded time for last publish
						  DEBUG_PRINT(" => no data change but max time interval exceeded => published on ");DEBUG_PRINTLN(MQTT_TOPIC);
						  MQTTClient.publish(MQTT_TOPIC,JSON);                             			// publish on MQTT server
						  matrix[i].last_published = millis();                               		// memorize published time
						} else {
						  DEBUG_PRINTLN(" => no data change => not published");
						}
					  }   // no data changed
					break;
					}     // authorized id
				}       // for
			// No ID filtering
			} else {
				MQTTClient.publish(MQTT_TOPIC,JSON);                   	// publish on MQTT server
			}
			
			//pubFlatMqtt(MQTT_TOPIC,JSON);								            // expands JSON and publish on several top
			
		} // end of DataReady

		now = millis();
		
		// Handle uptime
		if ( (now - lastUptime) > (uptimeInterval) ) {    				// if uptime interval is exceeded
			char mqtt_publish_payload[50];
			lastUptime = now;
			sprintf(mqtt_publish_payload,"%ld", uptime_min());
			MQTTClient.publish(MQTT_UPTIME_TOPIC,mqtt_publish_payload);
			DEBUG_PRINT("Uptime : ");DEBUG_PRINTLN(time_string_exp(millis()));
		}
		  
		// Handle Mega reset if no data is received
		if (resetMegaInterval > 0) {										// only if enabled
			if ( (now - lastReceived) > (resetMegaInterval) ) {    			// if time interval exceeded, reset Mega
				resetMegaCounter += 1;
				/*#if defined(MQTT_MEGA_RESET_TOPIC)
				char mqtt_publish_payload[50];
				sprintf(mqtt_publish_payload,"%ld", resetMegaCounter);
				MQTTClient.publish(MQTT_MEGA_RESET_TOPIC,mqtt_publish_payload);
				#endif*/
				DEBUG_PRINT("No data received for ");DEBUG_PRINT(time_string_exp(now - lastReceived));DEBUG_PRINTLN(": Resetting Mega");
				#if defined(MQTT_MEGA_RESET_TOPIC)
				  MQTTClient.publish(MQTT_MEGA_RESET_TOPIC,"1");
				  delay(1000);
				  MQTTClient.publish(MQTT_MEGA_RESET_TOPIC,"0");
				#endif
				delay(200);
				pinMode(MEGA_RESET_PIN, OUTPUT);
				delay(200);
				digitalWrite(MEGA_RESET_PIN,false);                         // Change the state of pin to ground
				delay(1000); 
				digitalWrite(MEGA_RESET_PIN,true);                          // Change the state of pin to VCC
				delay(50);
				lastReceived = millis();									// Fake the last received time to avoid permanent reset
			}
		}

		// Handle MQTT callback
		MQTTClient.loop();
		
	} // else (=connected on MQTT server)
	
	ArduinoOTA.handle();                                            // Listen for OTA update
	httpserver.handleClient();                                      // Listen for HTTP requests from clients
	
} // loop
