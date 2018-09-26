#include "Common.h"
#include "ArduinoJson.h"
#include "Rflink.h"

struct USER_ID_STRUCT {
  char id[MAX_DATA_LEN];        // ID
  long publish_interval;        // interval to publish (ms) if data did not change
  char description[60];         // description
};

struct USER_CMD_STRUCT {        // Preconfigured commands to show on web interface
  char text[MAX_DATA_LEN];      // button text
  char command[BUFFER_SIZE];    // command to send
};

/*********************************************************************************
 * Parameters for IDs filtering and serial comm, see other parameters in Common.h
/*********************************************************************************/

#define USER_ID_NUMBER 0             // *MUST* be equal to USER_IDs number of lines OR set to 0 to publish all IDs with no condition


const USER_ID_STRUCT USER_IDs[] = {    // Configure IDs that will be forwared to MQTT server, interval time to force publish if data did not change (in ms), and description
  { "0001"    , 10 * 1000 * 60 , "Auriol V3 - Thermom&egrave;tre piscine (ID forc&eacute;e)"  },
  { "2A1C"    , 10 * 1000 * 60 , "Oregon Rain2 - Pluie"                                       },
};

const USER_CMD_STRUCT USER_CMDs[] = {    // Configure commands to show on web interface
  { "Ping"       ,  "10;ping;"                              },
  { "Version"    ,  "10;version;"                           },
  { "Status"     ,  "10;status;"                            },
  { "Reboot"     ,  "10;reboot;"                            },
};

// Serial configuration

  SoftwareSerial softSerial(4, 2, false,BUFFER_SIZE+2); // software RX from GPIO4/D2 pin (unused pin on ESP01), software TX to RFLink on GPIO2/D4 pin
  auto& debugSerialTX = Serial;                         // debugSerialTX is to show information - use Serial to write on hardware serial (ESP TX pin) 
  auto& rflinkSerialRX = Serial;                        // rflinkSerialRX is for data from RFLink - use softSerial to listen on software serial, use Serial to listen on hardware serial (ESP RX pin)
  auto& rflinkSerialTX = softSerial;                    // rflinkSerialTX is for data to RFLink - use softSerial to write on software serial, use Serial to write on hardware serial (ESP TX pin)

/*********************************************************************************
 * Global Variables
/*********************************************************************************/

bool MQTT_DEBUG = 0;                   // debug variable to publish received data on MQTT debug topic ; default is disabled, can be enabled from web interface

long lastUptime = - 5 * 1000 * 60;     // timer to publish uptime on MQTT server ; sufficient negative value forces update at startup
long uptimeInterval = 5 * 1000 * 60;   // publish uptime every 5 min
long now;

struct USER_ID_STRUCT_ALL {
  char id[8];                 // ID
  long publish_interval;      // interval to publish (ms) if data did not change
  char description[60];       // description
  char json[BUFFER_SIZE];     // store last json message
  long last_published;        // store last publish (millis)
  long last_received;         // store last received (millis)
};

USER_ID_STRUCT_ALL matrix[USER_ID_NUMBER];

// main input / output buffers
char BUFFER [BUFFER_SIZE];
char BUFFER_DEBUG [BUFFER_SIZE + 5 + MAX_DATA_LEN + 5 + MAX_DATA_LEN + 5 + MAX_TOPIC_LEN + 5];
char JSON   [BUFFER_SIZE];

// message builder buffers
char MQTT_NAME[MAX_DATA_LEN];
char MQTT_ID  [MAX_DATA_LEN];
char MQTT_TOPIC[MAX_TOPIC_LEN];
char FIELD_BUF[MAX_DATA_LEN];

// Serial iterator counter
int CPT;

WiFiClient espClient;
PubSubClient MQTTClient(MQTT_SERVER, MQTT_PORT, callback, espClient);

ESP8266WebServer httpserver(80);                               // Create a webserver object that listens for HTTP request on port 80
ESP8266HTTPUpdateServer httpUpdater;                           // Firmware webupdate

void setup_wifi() {
        delay(10);
        //debugSerialTX.print("Connecting to ");
        //debugSerialTX.print(WIFI_SSID); debugSerialTX.println(" ...");
        WiFi.hostname(MQTT_RFLINK_CLIENT_NAME);
        WiFi.begin(WIFI_SSID,WIFI_PASSWORD);                                  // Connect to the network
        int i = 0;
        while (WiFi.status() != WL_CONNECTED) {                               // Wait for the Wi-Fi to connect
          delay(1000);
          //debugSerialTX.print(++i); debugSerialTX.print(' ');
        }
        //debugSerialTX.println('\n');
        //debugSerialTX.println("WiFi connected");
        //debugSerialTX.print("IP address:\t ");
        //debugSerialTX.println(WiFi.localIP());
}

/**
 * callback to handle rflink order received from MQTT subscribtion
 */
 
void callback(char* topic, byte* payload, unsigned int len) {
        rflinkSerialTX.write(payload, len);
        rflinkSerialTX.print(F("\r\n"));
        //debugSerialTX.println(F("=== MQTT command ==="));
        //debugSerialTX.print(F("message = "));
        //debugSerialTX.write(payload, len);
        //debugSerialTX.print(F("\r\n"));
}

/**
 * build MQTT topic name to pubish to using parsed NAME and ID from rflink message
 */
 
void buildMqttTopic(char *name, char *ID) {
        MQTT_TOPIC[0] = '\0';
        strcat(MQTT_TOPIC,MQTT_PUBLISH_TOPIC);
        strcat(MQTT_TOPIC,"/");
        strcat(MQTT_TOPIC,MQTT_NAME);
        strcat(MQTT_TOPIC,"-");
        strcat(MQTT_TOPIC,MQTT_ID);;
        strcat(MQTT_TOPIC,"\0");
}

/**
 * send formated message to serial
 */
 
void printToSerial() {
        debugSerialTX.println();
        debugSerialTX.println(F("=== RFLink packet ==="));
        debugSerialTX.print(F("Raw data = ")); debugSerialTX.print(BUFFER);
        debugSerialTX.print(F("MQTT topic = "));
        debugSerialTX.print(MQTT_TOPIC);
        debugSerialTX.print("/ => ");
        debugSerialTX.println(JSON);
        debugSerialTX.println();
}

/**
 * try to connect to MQTT Server
 */
 
boolean MqttConnect() {

        // connect to Mqtt server and subcribe to order topic
        if (MQTTClient.connect(MQTT_RFLINK_CLIENT_NAME, MQTT_USER, MQTT_PASSWORD, MQTT_WILL_TOPIC, 0, 1, MQTT_WILL_OFFLINE)) { // XXX
                MQTTClient.subscribe(MQTT_RFLINK_ORDER_TOPIC);
                MQTTClient.publish(MQTT_WILL_TOPIC,MQTT_WILL_ONLINE,1);   // XXX once connected, update status of will topic
        }

        // report mqtt connection status
        debugSerialTX.print(F("MQTT connection state : "));
        debugSerialTX.println(MQTTClient.state());
        return MQTTClient.connected();
}

/**
 * OTA
 */
 
void SetupOTA() {

  // ArduinoOTA.setPort(8266);                                      // Port defaults to 8266
  ArduinoOTA.setHostname(MQTT_RFLINK_CLIENT_NAME);                  // Hostname defaults to esp8266-[ChipID]
  
  ArduinoOTA.onStart([]() {
    debugSerialTX.println("Start OTA");
  });

  ArduinoOTA.onEnd([]() {
    debugSerialTX.println("End OTA");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    debugSerialTX.printf("Progress: %u%%\n", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    debugSerialTX.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) debugSerialTX.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) debugSerialTX.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) debugSerialTX.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) debugSerialTX.println("Receive Failed");
    else if (error == OTA_END_ERROR) debugSerialTX.println("End Failed");
  });

};
 
/**
 * HTTP server
 */
  
char cssEspEasy[] = ""     // CSS
	""     // from ESP Easy Mega release 20180914
	"* {font-family: sans-serif; font-size: 12pt; margin: 0px; padding: 0px; box-sizing: border-box; }h1 {font-size: 16pt; color: #07D; margin: 8px 0; font-weight: bold; }h2 {font-size: 12pt; margin: 0 -4px; padding: 6px; background-color: #444; color: #FFF; font-weight: bold; }h3 {font-size: 12pt; margin: 16px -4px 0 -4px; padding: 4px; background-color: #EEE; color: #444; font-weight: bold; }h6 {font-size: 10pt; color: #07D; }pre, xmp, code, kbd, samp, tt{ font-family:monospace,monospace; font-size:1em; }.button {margin: 4px; padding: 4px 16px; background-color: #07D; color: #FFF; text-decoration: none; border-radius: 4px; border: none;}.button.link { }.button.link.wide {display: inline-block; width: 100%; text-align: center;}.button.link.red {background-color: red;}.button.help {padding: 2px 4px; border-style: solid; border-width: 1px; border-color: gray; border-radius: 50%; }.button:hover {background: #369; }input, select, textarea {margin: 4px; padding: 4px 8px; border-radius: 4px; background-color: #eee; border-style: solid; border-width: 1px; border-color: gray;}input:hover {background-color: #ccc; }input.wide {max-width: 500px; width:80%; }input.widenumber {max-width: 500px; width:100px; }#selectwidth {max-width: 500px; width:80%; padding: 4px 8px;}select:hover {background-color: #ccc; }.container {display: block; padding-left: 35px; margin-left: 4px; margin-top: 0px; position: relative; cursor: pointer; font-size: 12pt; -webkit-user-select: none; -moz-user-select: none; -ms-user-select: none; user-select: none; }.container input {position: absolute; opacity: 0; cursor: pointer;  }.checkmark {position: absolute; top: 0; left: 0; height: 25px;  width: 25px;  background-color: #eee; border-style: solid; border-width: 1px; border-color: gray;  border-radius: 4px;}.container:hover input ~ .checkmark {background-color: #ccc; }.container input:checked ~ .checkmark { background-color: #07D; }.checkmark:after {content: ''; position: absolute; display: none; }.container input:checked ~ .checkmark:after {display: block; }.container .checkmark:after {left: 7px; top: 3px; width: 5px; height: 10px; border: solid white; border-width: 0 3px 3px 0; -webkit-transform: rotate(45deg); -ms-transform: rotate(45deg); transform: rotate(45deg); }.container2 {display: block; padding-left: 35px; margin-left: 9px; margin-bottom: 20px; position: relative; cursor: pointer; font-size: 12pt; -webkit-user-select: none; -moz-user-select: none; -ms-user-select: none; user-select: none; }.container2 input {position: absolute; opacity: 0; cursor: pointer;  }.dotmark {position: absolute; top: 0; left: 0; height: 26px;  width: 26px;  background-color: #eee; border-style: solid; border-width: 1px; border-color: gray; border-radius: 50%;}.container2:hover input ~ .dotmark {background-color: #ccc; }.container2 input:checked ~ .dotmark { background-color: #07D;}.dotmark:after {content: ''; position: absolute; display: none; }.container2 input:checked ~ .dotmark:after {display: block; }.container2 .dotmark:after {top: 8px; left: 8px; width: 8px; height: 8px; border-radius: 50%; background: white; }#toastmessage {visibility: hidden; min-width: 250px; margin-left: -125px; background-color: #07D;color: #fff;  text-align: center;  border-radius: 4px;  padding: 16px;  position: fixed;z-index: 1; left: 282px; bottom: 30%;  font-size: 17px;  border-style: solid; border-width: 1px; border-color: gray;}#toastmessage.show {visibility: visible; -webkit-animation: fadein 0.5s, fadeout 0.5s 2.5s; animation: fadein 0.5s, fadeout 0.5s 2.5s; }@-webkit-keyframes fadein {from {bottom: 20%; opacity: 0;} to {bottom: 30%; opacity: 0.9;} }@keyframes fadein {from {bottom: 20%; opacity: 0;} to {bottom: 30%; opacity: 0.9;} }@-webkit-keyframes fadeout {from {bottom: 30%; opacity: 0.9;} to {bottom: 0; opacity: 0;} }@keyframes fadeout {from {bottom: 30%; opacity: 0.9;} to {bottom: 0; opacity: 0;} }.level_0 { color: #F1F1F1; }.level_1 { color: #FCFF95; }.level_2 { color: #9DCEFE; }.level_3 { color: #A4FC79; }.level_4 { color: #F2AB39; }.level_9 { color: #FF5500; }.logviewer {  color: #F1F1F1; background-color: #272727;  font-family: 'Lucida Console', Monaco, monospace;  height:  530px; max-width: 1000px; width: 80%; padding: 4px 8px;  overflow: auto;   border-style: solid; border-color: gray; }textarea {max-width: 1000px; width:80%; padding: 4px 8px; font-family: 'Lucida Console', Monaco, monospace; }textarea:hover {background-color: #ccc; }table.normal th {padding: 6px; background-color: #444; color: #FFF; border-color: #888; font-weight: bold; }table.normal td {padding: 4px; height: 30px;}table.normal tr {padding: 4px; }table.normal {color: #000; width: 100%; min-width: 420px; border-collapse: collapse; }table.multirow th {padding: 6px; background-color: #444; color: #FFF; border-color: #888; font-weight: bold; }table.multirow td {padding: 4px; text-align: center;  height: 30px;}table.multirow tr {padding: 4px; }table.multirow tr:nth-child(even){background-color: #DEE6FF; }table.multirow {color: #000; width: 100%; min-width: 420px; border-collapse: collapse; }tr.highlight td { background-color: #dbff0075; }.note {color: #444; font-style: italic; }.headermenu {position: fixed; top: 0; left: 0; right: 0; height: 90px; padding: 8px 12px; background-color: #F8F8F8; border-bottom: 1px solid #DDD; z-index: 1;}.apheader {padding: 8px 12px; background-color: #F8F8F8;}.bodymenu {margin-top: 96px;}.menubar {position: inherit; top: 55px; }.menu {float: left; padding: 4px 16px 8px 16px; color: #444; white-space: nowrap; border: solid transparent; border-width: 4px 1px 1px; border-radius: 4px 4px 0 0; text-decoration: none; }.menu.active {color: #000; background-color: #FFF; border-color: #07D #DDD #FFF; }.menu:hover {color: #000; background: #DEF; }.menu_button {display: none;}.on {color: green; }.off {color: red; }.div_l {float: left; }.div_r {float: right; margin: 2px; padding: 1px 10px; border-radius: 4px; background-color: #080; color: white; }.div_br {clear: both; }.alert {padding: 20px; background-color: #f44336; color: white; margin-bottom: 15px; }.warning {padding: 20px; background-color: #ffca17; color: white; margin-bottom: 15px; }.closebtn {margin-left: 15px; color: white; font-weight: bold; float: right; font-size: 22px; line-height: 20px; cursor: pointer; transition: 0.3s; }.closebtn:hover {color: black; }section{overflow-x: auto; width: 100%; }@media screen and (max-width: 960px) {span.showmenulabel { display: none; }.menu { max-width: 11vw; max-width: 48px; }\r\n";
  
char cssDatasheet[] = ""     // CSS
		// force some changes in CSS compared to ESP Easy Mega
	"<style>table.multirow td {text-align: left; font-size:0.9em;} h3 {margin: 16px 0px 0px 0px;}\r\n"
	"table.condensed td {padding: 0px 20px 0px 5px; height: 1em;}table.condensed tr {padding: 0px; }table.condensed {padding: 0px;border-left:1px solid #EEE;}\r\n"
	"</style>\r\n";
	
void ConfigHTTPserver() {

  httpserver.on("/esp.css",[](){       // CSS EspEasy
  
    String cssMessage = String(cssEspEasy);
	httpserver.send(200, "text/css", cssMessage);
	
  });
	
  httpserver.on("/",[](){       // Main, page d'accueil
  
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
    htmlMessage += "<div class='menubar'><a class='menu active' href='.'>&#8962;<span class='showmenulabel'>Main</span></a>\r\n";
    htmlMessage += "<a class='menu' href='livedata'>&#10740;<span class='showmenulabel'>Live Data</span></a>\r\n";
    htmlMessage += "<a id='resetmega' class='menu' href='reset-mega'>&#128204;<span class='showmenulabel'>Reset MEGA</span></a>\r\n";
    htmlMessage += "<script>document.getElementById(\"resetmega\").onclick = function(e) {\r\n"
		"var wnd = window.open(\"reset-mega\")\r\n"
		"wnd.close();e.preventDefault();};</script>";
    htmlMessage += "<a class='menu' id='reboot' href='reboot'>&#128268;<span class='showmenulabel'>Reboot ESP</span></a>\r\n";
    htmlMessage += "<script>document.getElementById(\"reboot\").onclick = function(e) {\r\n"
		"var wnd = window.open(\"reboot\")\r\n"
		"wnd.close();e.preventDefault();"
		"setTimeout(function(){ location.reload(); }, 5000);"
		"};"
		"</script>";
    htmlMessage += (!MQTT_DEBUG)? "<a class='menu' href='enable-debug'>&#128172;<span class='showmenulabel'>Enable MQTT debug topic</span></a>\r\n":"<a class='menu' href='disable-debug'>&#128172;<span class='showmenulabel'>Disable MQTT debug topic</span></a>\r\n";
    htmlMessage += "<a class='menu' href='update'>&#128295;<span class='showmenulabel'>Load ESP firmware</span></a></div>\r\n";
    htmlMessage += "</header>\r\n";	
    
    // Live data
    htmlMessage += "<h3>RFLink Live Data *</h3>\r\n";

    //htmlMessage += "<iframe name=\"livedataframe\" id=\"livedataframe\" frameborder=\"0\" border=\"0\" cellspacing=\"0\" src=\"live-data\" style=\"height:25em;width:100%;border:none;\"></iframe>\r\n";

    htmlMessage += "<input type=\"button\" value =\"Pause\" onclick=\"stopUpdate();\" />";                                                                // Pause
    htmlMessage += "<input type=\"button\" value =\"Restart\" onclick=\"restartUpdate();\" />";                                                           // Restart
    htmlMessage += "<input type=\"button\" value =\"Refresh\" onclick=\"window.location.reload(true);\" />\r\n";                                          // Refresh
    //htmlMessage += "<form action=\"/send\" id=\"form_lines\" style=\"display:inline\">Number of lines (0-100) :";                                         // Lines number
    //htmlMessage += "<input type=\"number\" name=\"live_data_lines\" min=\"0\" max=\"100\" step=\"10\" value=\"" + String(LIVE_DATA_LINES) + "\">";
    //htmlMessage += "<input type=\"submit\" value=\"OK\"></form>\r\n";                                                                                     // OK
    htmlMessage += "<input type=\"text\" id=\"mySearch\" onkeyup=\"filterLines()\" placeholder=\"Search for...\" title=\"Type in a name\"><br />\r\n";    // Search
    
    htmlMessage += "<table id=\"liveData\" class='multirow';>\r\n";                                                                                       // Table of x lines
    htmlMessage += "<tr class=\"header\"><th style='text-align:left;'>Raw Data</th><th style='text-align:left;'> MQTT Topic </th><th style='text-align:left;'> MQTT JSON </th></tr>\r\n";
    for (int i = 0; i < (5); i++){ // fix number instead of LIVE_DATA_LINES to avoid overflow
      htmlMessage += "<tr id=\"data" + String(i) + "\"><td></td><td></td><td></td></tr>\r\n";
    } 
    htmlMessage += "</table>\r\n";
    
    htmlMessage += "<script>\r\n";                                                       // Script to filter lines
    htmlMessage += "function filterLines() {\r\n";
    htmlMessage += "  var input, filter, table, tr, td, i;\r\n";
    htmlMessage += "  input = document.getElementById(\"mySearch\");\r\n";
    htmlMessage += "  filter = input.value.toUpperCase();\r\n";
    htmlMessage += "  table = document.getElementById(\"liveData\");\r\n";
    htmlMessage += "  tr = table.getElementsByTagName(\"tr\");\r\n";
    htmlMessage += "  for (i = 0; i < tr.length; i++) {\r\n";
    htmlMessage += "    td = tr[i].getElementsByTagName(\"td\")[0];\r\n";
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
    for (int i = (5 - 1); i > 0; i--){	// fix number instead of LIVE_DATA_LINES to avoid overflow
      htmlMessage += "document.getElementById(\"data" + String(i) + "\").innerHTML = document.getElementById(\"data" + String (i-1) + "\").innerHTML;\r\n";
    }
    htmlMessage += "}\r\n";
    htmlMessage += "document.getElementById(\"data0\").innerHTML = this.responseText;\r\n";
    htmlMessage += "memorized_data = this.responseText;\r\n"; // memorize new data
    htmlMessage += "filterLines();\r\n";                      // apply filter from mySearch input
    htmlMessage += "}\r\n";
    htmlMessage += "function stopUpdate(){\r\n";
    htmlMessage += " clearInterval(x);\r\n";
    htmlMessage += "}\r\n";
    htmlMessage += "function restartUpdate(){\r\n";
    htmlMessage += " x = setInterval(function() {loadData(\"data.txt\",updateData)}, 500);\r\n";       // update every 500 ms
    htmlMessage += "}\r\n";
    htmlMessage += "</script>\r\n";
    htmlMessage += "<div class='note'>* see Live \"Data tab\" for more lines - web view may not catch all frames, MQTT debug is more accurate</div>\r\n";
    //htmlMessage += "<br />\r\n";
    
    // Commands to RFLink
    htmlMessage += "<h3>Commands to RFLink</h3><br />";
    htmlMessage += "<form action=\"/send\" id=\"form_command\" style=\"float: left;\"><input type=\"text\" id=\"command\" name=\"command\">";
    htmlMessage += "<input type=\"submit\" value=\"Send\"><a class='button help' href='http://www.rflink.nl/blog2/protref' target='_blank'>&#10068;</a></form>\r\n";
	htmlMessage += "<script>function submitForm() {"
		"var http = new XMLHttpRequest();"
		"http.open(\"POST\", \"/send\", true);"
		"http.setRequestHeader(\"Content-type\",\"application/x-www-form-urlencoded\");"
		"var params = \"command=\" + document.getElementById('command').value;"
		"http.send(params);"
		//"http.onload = function() {"
		//"	alert(http.responseText);"
		//"}"
		"}</script>";
    for (int i = 0; i < (sizeof(USER_CMDs) / sizeof(USER_CMDs[0])); i++){      // User commands defined in USER_CMDs for quick access
      htmlMessage += "<a class='button link' style=\"float: left;\" href=\"javascript:{}\""; 
      htmlMessage += "onclick=\"document.getElementById('command').value = '" + String(USER_CMDs[i].command) + "';";
      //htmlMessage += "document.getElementById('form_command').submit(); return false;\">" + String(USER_CMDs[i].text) + "</a>\r\n";
      htmlMessage += "submitForm(); return false;\">" + String(USER_CMDs[i].text) + "</a>\r\n";  
	}
    htmlMessage += "<br style=\"clear: both;\"/>\r\n";

    // System Info
    htmlMessage += "<h3>System Info</h3>\r\n";
    htmlMessage += "<table class='normal'>\r\n";
    htmlMessage += "<tr><td style='min-width:150px;'>Version</td><td style='width:80%;'>20180922_12</td></tr>\r\n";
    htmlMessage += "<tr><td>Uptime</td><td>" + String(uptime_string_exp()) + "</td></tr>\r\n";
    htmlMessage += "<tr><td>WiFi network</td><td>" + String(WiFi.SSID()) + " (" + WiFi.BSSIDstr() + ")</td></tr>\r\n";
    htmlMessage += "<tr><td>WiFi RSSI</td><td>" + String(WiFi.RSSI()) + " dB</td></tr>\r\n";
    htmlMessage += "<tr><td>IP address (MAC)</td><td>" + WiFi.localIP().toString() +" (" + String(WiFi.macAddress()) +")</td></tr>\r\n";
    htmlMessage += "<tr><td>ESP pin to reset MEGA</td><td>GPIO " + String(MEGA_RESET_PIN) + "</td></tr>";
    htmlMessage += "<tr><td>MQTT server and port</td><td>" + String(MQTT_SERVER) + ":" + String(MQTT_PORT)+"</td></tr>\r\n";
    htmlMessage += "<tr><td>MQTT debug topic</td><td>";
    (MQTT_DEBUG)? htmlMessage += "<span style=\"font-weight:bold\">enabled</span>" : htmlMessage += "disabled";
    htmlMessage += "</td></tr>\r\n";
    htmlMessage += "<tr><td>MQTT connection state <a class='button help' href='https://pubsubclient.knolleary.net/api.html#state' target='_blank'>&#10068;</a></td><td>" + String(MQTTClient.state()) + "</td></tr>\r\n";
    //htmlMessage += "<tr><td>MQTT topics</td><td style=\"font-family: 'Courier New', Courier, monospace;\">\r\n";
    htmlMessage += "<tr><td>MQTT topics</td><td><table class='condensed'>\r\n";
    htmlMessage += "<tr><td>publish (json)</td><td> " + String(MQTT_PUBLISH_TOPIC) + "/Protocol_Name-ID</td></tr>\r\n";
    htmlMessage += "<tr><td>commands to rflink</td><td> " + String(MQTT_RFLINK_ORDER_TOPIC) + "</td></tr>\r\n";
    htmlMessage += "<tr><td>last will ( " + String(MQTT_WILL_ONLINE) + " / " + String(MQTT_WILL_OFFLINE) + " )</td><td> " + String(MQTT_WILL_TOPIC) + "</td></tr>\r\n";
    htmlMessage += "<tr><td>uptime (min, every " + String( int( float(uptimeInterval) *0.001 / 60) ) + ")</td><td> " + String(MQTT_UPTIME_TOPIC) + "</td></tr>\r\n";
    htmlMessage += "<tr><td>debug (data from rflink)</td><td> " + String(MQTT_DEBUG_TOPIC) + "</td></tr>\r\n";
    htmlMessage += "</table></td></tr>\r\n";
    htmlMessage += "<tr><td>User specific</td><td>ID for protocol Auriol V3 is forced to 0001</td></tr>\r\n";
    htmlMessage += "</table><br />\r\n";

    // User ID
    htmlMessage += "<h3>User IDs configuration *</h3>\r\n";
    if (USER_ID_NUMBER > 0) {                                                                         // show list of user IDs
      int i;
      htmlMessage += "<table class='multirow'><tr><th>ID</th><th>Description</th><th>Interval</th><th>Received</th><th>Published</th><th style='text-align:left;'>Last MQTT JSON</th></tr>\r\n";
      for (i = 0; i < (USER_ID_NUMBER); i++){
        htmlMessage += "<tr><td style=\"text-align: center;\">" + String(matrix[i].id) + "</td>";
        htmlMessage += "<td style=\"text-align: left;\">" + String(matrix[i].description) + "</td>";
        htmlMessage += "<td style=\"text-align: center;\">" + String( int(float(matrix[i].publish_interval) *0.001 /60)) + " min</td>";
        if (matrix[i].last_received != 0) {
          htmlMessage += "<td style=\"text-align: center;\">" + String( int( (float(now) - float(matrix[i].last_received)) *0.001 /60 ) ) + " min ago</td>";
        } else {
          htmlMessage += "<td style=\"text-align: center;\"></td>";
        }
        if (matrix[i].last_published != 0) {
          htmlMessage += "<td style=\"text-align: center;\">" + String( int( (float(now) - float(matrix[i].last_published)) *0.001 /60 ) ) + " min ago</td>";
        } else {
          htmlMessage += "<td style=\"text-align: center;\"></td>";
        }
        htmlMessage += "<td>" + String(matrix[i].json) + "</td></tr>\r\n";
      }
      htmlMessage += "</table>\r\n";
      htmlMessage += "<div class='note'>* only those IDs are published on MQTT server, and only if data changed or interval time is exceeded</div>\r\n";
      //htmlMessage += "<a id=\"end\"></a>";
    } else {
      htmlMessage += "<div class='note'>All IDs are forwarded to MQTT server</div>\r\n";
    }
    htmlMessage += "<span style=\"font-size: 0.9em\">Running for " + String( int(float(now) *0.001 /60)) + " minutes </span>\r\n";
    htmlMessage += "<input type=\"button\" value =\"Refresh\" onclick=\"window.location.reload(true);\" />\r\n";
    htmlMessage += "<footer><br><h6>Powered by <a href='http://www.letscontrolit.com' style='font-size: 15px; text-decoration: none'>www.letscontrolit.com</a></h6></footer>\r\n";
    htmlMessage += "</body>\r\n</html>\r\n";
    httpserver.send(200, "text/html", htmlMessage);
  }); // server.on("/"...



  httpserver.on("/livedata",[](){       // 

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
    htmlMessage += "<a class='menu active' href='livedata'>&#10740;<span class='showmenulabel'>Live Data</span></a>\r\n";
    htmlMessage += "<a id='resetmega' class='menu' href='#'>&#128204;<span class='showmenulabel'>Reset MEGA</span></a>\r\n";
    htmlMessage += "<script>document.getElementById(\"resetmega\").onclick = function(e) {\r\n"
		"var wnd = window.open(\"reset-mega\")\r\n"
		"wnd.close();e.preventDefault();};</script>";
    htmlMessage += "<a class='menu' id='reboot' href='reboot'>&#128268;<span class='showmenulabel'>Reboot ESP</span></a>\r\n";
    htmlMessage += "<script>document.getElementById(\"reboot\").onclick = function(e) {\r\n"
		"var wnd = window.open(\"reboot\")\r\n"
		"wnd.close();e.preventDefault();"
		// onreload on main page // "setTimeout(function(){ location.reload(); }, 5000);" //
		"};"
		"</script>";
    htmlMessage += (!MQTT_DEBUG)? "<a class='menu' href='enable-debug'>&#128172;<span class='showmenulabel'>Enable MQTT debug topic</span></a>\r\n":"<a class='menu' href='disable-debug'>&#128172;<span class='showmenulabel'>Disable MQTT debug topic</span></a>\r\n";
    htmlMessage += "<a class='menu' href='update'>&#128295;<span class='showmenulabel'>Load ESP firmware</span></a></div>\r\n";
    htmlMessage += "</header>\r\n";
    
    // Live data
    htmlMessage += "<h3>RFLink Live Data</h3>\r\n";

    //htmlMessage += "<iframe name=\"livedataframe\" id=\"livedataframe\" frameborder=\"0\" border=\"0\" cellspacing=\"0\" src=\"live-data\" style=\"height:25em;width:100%;border:none;\"></iframe>\r\n";

    htmlMessage += "<input type=\"button\" value =\"Pause\" onclick=\"stopUpdate();\" />";                                                                // Pause
    htmlMessage += "<input type=\"button\" value =\"Restart\" onclick=\"restartUpdate();\" />";                                                           // Restart
    htmlMessage += "<input type=\"button\" value =\"Refresh\" onclick=\"window.location.reload(true);\" />\r\n";                                          // Refresh
    //htmlMessage += "<form action=\"/send\" id=\"form_lines\" style=\"display:inline\">Number of lines (0-100) :";                                         // Lines number
    //htmlMessage += "<input type=\"number\" name=\"live_data_lines\" min=\"0\" max=\"100\" step=\"10\" value=\"" + String(LIVE_DATA_LINES) + "\">";
    //htmlMessage += "<input type=\"submit\" value=\"OK\"></form>\r\n";                                                                                     // OK
    htmlMessage += "<input type=\"text\" id=\"mySearch\" onkeyup=\"filterLines()\" placeholder=\"Search for...\" title=\"Type in a name\"><br />\r\n";    // Search
    
    htmlMessage += "<table id=\"liveData\" class='multirow';>\r\n";                                                                                       // Table of x lines
    htmlMessage += "<tr class=\"header\"><th style='text-align:left;'>Raw Data</th><th style='text-align:left;'> MQTT Topic </th><th style='text-align:left;'> MQTT JSON </th></tr>\r\n";
    //for (int i = 0; i < (LIVE_DATA_LINES); i++){
    //for (int i = 0; i < 1; i++){
	htmlMessage += "<tr id=\"data0" "\"><td></td><td></td><td></td></tr>\r\n";
    //} 
    htmlMessage += "</table>\r\n";
    
    htmlMessage += "<script>\r\n";                                                       // Script to filter lines
    htmlMessage += "function filterLines() {\r\n";
    htmlMessage += "  var input, filter, table, tr, td, i;\r\n";
    htmlMessage += "  input = document.getElementById(\"mySearch\");\r\n";
    htmlMessage += "  filter = input.value.toUpperCase();\r\n";
    htmlMessage += "  table = document.getElementById(\"liveData\");\r\n";
    htmlMessage += "  tr = table.getElementsByTagName(\"tr\");\r\n";
    htmlMessage += "  for (i = 0; i < tr.length; i++) {\r\n";
    htmlMessage += "    td = tr[i].getElementsByTagName(\"td\")[0];\r\n";
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
    //for (int i = (LIVE_DATA_LINES - 1); i > 0; i--){
    //  htmlMessage += "document.getElementById(\"data" + String(i) + "\").innerHTML = document.getElementById(\"data" + String (i-1) + "\").innerHTML;\r\n";
    //}
    htmlMessage += "}\r\n";
    htmlMessage += "document.getElementById(\"data0\").innerHTML = this.responseText;\r\n";
    htmlMessage += "memorized_data = this.responseText;\r\n"; // memorize new data
    htmlMessage += "filterLines();\r\n";                      // apply filter from mySearch input
    htmlMessage += "}\r\n";
    htmlMessage += "function stopUpdate(){\r\n";
    htmlMessage += " clearInterval(x);\r\n";
    htmlMessage += "}\r\n";
    htmlMessage += "function restartUpdate(){\r\n";
    htmlMessage += " x = setInterval(function() {loadData(\"data.txt\",updateData)}, 250);\r\n";       // update every 250 ms
    htmlMessage += "}\r\n";
    htmlMessage += "</script>\r\n";
    
    htmlMessage += "<span style=\"font-size: 0.9em\">Running for " + String( int(float(now) *0.001 /60)) + " minutes </span>\r\n";
    htmlMessage += "<input type=\"button\" value =\"Refresh\" onclick=\"window.location.reload(true);\" />\r\n";
    htmlMessage += "<footer><br><h6>Powered by <a href='http://www.letscontrolit.com' style='font-size: 15px; text-decoration: none'>www.letscontrolit.com</a></h6></footer>\r\n";
    htmlMessage += "</body>\r\n</html>\r\n";
    httpserver.send(200, "text/html", htmlMessage);
	
  }); // livedata
  
  httpserver.on("/reboot",[](){                                	// Reboot ESP
    debugSerialTX.println("Rebooting device...");
    //httpserver.sendHeader("Location","/");                    // Add a header to respond with a new location for the browser to go to the home page again
    //httpserver.send(303);                                     // Send it back to the browser with an HTTP status 303 (See Other) to redirect
    httpserver.send(200, "text/html", "Rebooting...");
	delay(50);
    ESP.restart();
    //ESP.reset();
  });

  httpserver.on("/reset-mega",[](){                             // Reset MEGA
    debugSerialTX.println("Resetting Mega...");
    pinMode(MEGA_RESET_PIN, OUTPUT);
    digitalWrite(MEGA_RESET_PIN,false);                         // Change the state of pin to ground
    delay(1000); 
    digitalWrite(MEGA_RESET_PIN,true);                          // Change the state of pin VCC
	delay(50);
    //httpserver.sendHeader("Location","/");                    // Add a header to respond with a new location for the browser to go to the home page again
    //httpserver.send(303);                                     // Send it back to the browser with an HTTP status 303 (See Other) to redirect
    httpserver.send(200, "text/html", "Resetting Mega...");

	});

  httpserver.on("/send",[](){                                   // Handle inputs from web interface

    if (httpserver.args() > 0 ) {
        for ( uint8_t i = 0; i < httpserver.args(); i++ ) {
          
          if (httpserver.argName(i) == "command") {             // Send command to RFLInk from web interface, chekc it comes from command input in html form
             String text_command = httpserver.arg(i);           // Get command send
             byte buf[text_command.length() + 1];               // Temp char for conversion
             text_command.getBytes(buf, sizeof(buf));
             rflinkSerialTX.write(buf, sizeof(buf));            // Write command to RFLink serial
             //rflinkSerialTX.print(httpserver.arg(i));
             rflinkSerialTX.print(F("\r\n"));
             //debugSerialTX.println();
             //debugSerialTX.println(F("=== Web command ==="));
             //debugSerialTX.print(F("message = "));
             //debugSerialTX.write(buf, sizeof(buf));
             //debugSerialTX.print(httpserver.arg(i));
             //debugSerialTX.println(F("\r\n"));
          }
          
       }
    }
    httpserver.sendHeader("Location","/");                      // Add a header to respond with a new location for the browser to go to the home page again
    httpserver.send(303);                                       // Send it back to the browser with an HTTP status 303 (See Other) to redirect
    
  });
  
  httpserver.on("/enable-debug",[](){
    debugSerialTX.println("Enabling MQTT debug...");
    MQTT_DEBUG = 1;
    httpserver.sendHeader("Location","/");                      // Add a header to respond with a new location for the browser to go to the home page again
    httpserver.send(303);                                       // Send it back to the browser with an HTTP status 303 (See Other) to redirect
  });

  httpserver.on("/disable-debug",[](){
    debugSerialTX.println("Disabling MQTT debug...");
    MQTT_DEBUG = 0;
    httpserver.sendHeader("Location","/");                      // Add a header to respond with a new location for the browser to go to the home page again
    httpserver.send(303);                                       // Send it back to the browser with an HTTP status 303 (See Other) to redirect
  });
  
  httpserver.on("/data.txt", [](){                              // Used to deliver raw data received (BUFFER) and mqtt data published (MQTT_TOPIC and JSON)           
    httpserver.send(200, "text/html", "<td>" + String(BUFFER) + "</td><td>" + String(MQTT_TOPIC) + "</td><td>" + String(JSON) + "</td>\r\n");
  });

  httpserver.onNotFound([](){
    httpserver.send(404, "text/plain", "404: Not found");             // Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
  });

};

/**
 * MQTT publish without json
 */
 
void pubFlatMqtt(char* topic,char* json)                            // Unused but could be usefull
{
        DynamicJsonBuffer jsonBuffer(BUFFER_SIZE);
        JsonObject& root = jsonBuffer.parseObject(json);
        char myTopic[40];

        // Test if parsing succeeds.
        if (!root.success()) {
                debugSerialTX.println("parseObject() failed");
                return;
        }

        for (auto kv : root) {
                debugSerialTX.print(kv.key);
                debugSerialTX.print(F(" => "));
                debugSerialTX.println(kv.value.as<char*>());
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
 secs = millis()/1000; //convect milliseconds to seconds
 mins=secs/60; //convert seconds to minutes
 hours=mins/60; //convert minutes to hours
 days=hours/24; //convert hours to days
 return mins;
}

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
} 

/**
 * Miscellaneous
 */

boolean isNumeric(String str) {
    unsigned int stringLength = str.length();
    if (stringLength == 0) {
        return false;
    }
    boolean seenDecimal = false;
    for(unsigned int i = 0; i < stringLength; ++i) {
        if (isDigit(str.charAt(i))) {
            continue;
        }
        if (str.charAt(i) == '.') {
            if (seenDecimal) {
                return false;
            }
            seenDecimal = true;
            continue;
        }
        return false;
    }
    return true;
}

/*********************************************************************************
 * Classic arduino bootstrap
/*********************************************************************************/

void setup() {
   

        // Open serial communications and wait for port to open:
        
        //debugSerialTX.begin(115200);
        debugSerialTX.begin(57600);        // XXX Serial : same speed as RFLInk
        debugSerialTX.println();
        debugSerialTX.println(F("Starting..."));
        while (!debugSerialTX) {
                ; // wait for serial port to connect. Needed for native USB port only
        }
        debugSerialTX.println(F("Init serial done"));

        // set the data rate for the SoftwareSerial port
        rflinkSerialTX.begin(57600);
        debugSerialTX.println(F("Init software serial done"));

        // Setup WIFI
        setup_wifi();

        // Setup MQTT
        MqttConnect();

        // Setup OTA
        SetupOTA();
        ArduinoOTA.begin();
        debugSerialTX.print("Ready for OTA on ");
        debugSerialTX.print("IP address:\t");
        debugSerialTX.println(WiFi.localIP());

        // Setup HTTP Server
        ConfigHTTPserver();
        httpUpdater.setup(&httpserver);                                 // Firmware webupdate
        //debugSerialTX.printf("HTTPUpdateServer ready! Open http://%s.local/update in your browser\n", MQTT_RFLINK_CLIENT_NAME);
        debugSerialTX.println("HTTPUpdateServer ready");
        httpserver.begin();                                             // Actually start the HTTP server
        debugSerialTX.println("HTTP server started"); 
        
        rflinkSerialTX.println();
        //rflinkSerialTX.println(F("10;status;"));                          // Ask status to RFLink
        rflinkSerialTX.println(F("10;ping;"));                              // Do a PING on startup

        // get values from USER_IDs into matrix
        for (int i =0; i < USER_ID_NUMBER; i++) {
            strcpy(matrix[i].id,USER_IDs[i].id);
            matrix[i].publish_interval = USER_IDs[i].publish_interval;
            strcpy(matrix[i].description,USER_IDs[i].description);
        }

} // setup

/*********************************************************************************
 * Main loop
/*********************************************************************************/

void loop() {
        bool DataReady=false;
        // handle lost of connection : retry after 5s on each loop
        if (!MQTTClient.connected()) {
                debugSerialTX.println(F("Not connected, retrying in 5s"));
                delay(5000);
                MqttConnect();
        } else {
				int lastchar;
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
										  lastchar = CPT-1;
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
                        strcpy(MQTT_ID,""); strcpy(MQTT_NAME,"");strcpy(MQTT_TOPIC,"");strcpy(JSON,"");
						
                        readRfLinkPacket(BUFFER);
						
                        //if (strcmp(MQTT_NAME,"Auriol_V3") == 0) strcpy( MQTT_ID , "0001");  // XXX force unique ID for device changing frequently
                        
                        // construct topic name to publish to
                        buildMqttTopic(MQTT_NAME, MQTT_ID);
						

                        // report message for debugging
                        //printToSerial();
                        //if (MQTT_DEBUG) MQTTClient.publish(MQTT_DEBUG_TOPIC,BUFFER);      // XXX MQTT debug mode, raw data only
						if (MQTT_DEBUG) {													// XXX MQTT debug mode with json, full data
							strcpy(BUFFER_DEBUG,"{");
							BUFFER[lastchar-1] = ' ';BUFFER[lastchar] = ' ';										// Removes line return \n in BUFFER
							strcat(BUFFER_DEBUG,"\"DATA\":\"");strcat(BUFFER_DEBUG,BUFFER);
							strcat(BUFFER_DEBUG,"\",\"ID\":\"");strcat(BUFFER_DEBUG,MQTT_ID);
							strcat(BUFFER_DEBUG,"\",\"NAME\":\"");strcat(BUFFER_DEBUG,MQTT_NAME);
							strcat(BUFFER_DEBUG,"\",\"TOPIC\":\"");strcat(BUFFER_DEBUG,MQTT_TOPIC);
							// XXX todo: replace " or remove brackets {}- strcat(BUFFER_DEBUG,"\",\"JSON\":\"");strcat(BUFFER_DEBUG,JSON);
							strcat(BUFFER_DEBUG,"\"}");
							MQTTClient.publish(MQTT_DEBUG_TOPIC,BUFFER_DEBUG);
						}
                        
                        // XXX publish to MQTT server only if ID is authorized
                        
                        if (USER_ID_NUMBER > 0) {
                          
                          int i;
                          for (i = 0; i < (USER_ID_NUMBER); i++){
                            
                            //debugSerialTX.print("ID = ");debugSerialTX.print(matrix[i].id);
                            //debugSerialTX.print(" ; publish_interval = ");debugSerialTX.print(matrix[i].publish_interval);
                            //debugSerialTX.print(" ; decription = ");debugSerialTX.print(matrix[i].description);
                            //debugSerialTX.print(" ; json = ");debugSerialTX.print(matrix[i].json);
                            //debugSerialTX.print(" ; last_published = ");debugSerialTX.print(matrix[i].last_published);
                            //debugSerialTX.print(" ; last_received = ");debugSerialTX.println(matrix[i].last_received);
                            
                            if (strcmp(MQTT_ID,matrix[i].id) == 0) {                                      	// check ID is authorized
                              //debugSerialTX.print("Authorized ID ");debugSerialTX.print(MQTT_ID);
                              matrix[i].last_received = millis();                                         	// memorize received time
                              if (strcmp(matrix[i].json,JSON) != 0) {                                     	// check if json value has changed
                                //debugSerialTX.print(" => data changed => published on ");debugSerialTX.println(MQTT_TOPIC);
                                MQTTClient.publish(MQTT_TOPIC,JSON);                           			  	// if changed, publish on MQTT server  
                                strcpy(matrix[i].json,JSON);                                              	// memorize new json value
                                matrix[i].last_published = millis();                                      	// memorize published time                                
                              } else {                                                                    	// no value change
                                now = millis();
                                if ( (now - matrix[i].last_published) > (matrix[i].publish_interval) ) {   	// check if it exceeded time for last publish
                                  //debugSerialTX.print(" => no data change but max time interval exceeded => published on ");debugSerialTX.println(MQTT_TOPIC);
                                  MQTTClient.publish(MQTT_TOPIC,JSON);                             			// publish on MQTT server
                                  matrix[i].last_published = millis();                               		// memorize published time
                                } else {
                                  //debugSerialTX.println(" => no data change => not published");
                                }
                              }   // no data changed
                            }     // authorized id
                            
                          }       // for
                          
                        } else {                                                	// case all IDs are authorized
                          MQTTClient.publish(MQTT_TOPIC,JSON);                   	// publish on MQTT server
                        }
                        //debugSerialTX.println();
                        //pubFlatMqtt(MQTT_TOPIC,JSON);								// expands JSON and publish on several topics
                } // end of DataReady

                // Handle uptime
                now = millis();
                if ( (now - lastUptime) > (uptimeInterval) ) {    // if time interval exceeded, publish uptime
                  lastUptime = now;
                  char mqtt_publish_payload[5];
                  String mqtt_publish_string = String(uptime_min());
                  mqtt_publish_string.toCharArray(mqtt_publish_payload, mqtt_publish_string.length() + 1);
                  MQTTClient.publish(MQTT_UPTIME_TOPIC,mqtt_publish_payload);
                  //debugSerialTX.print("Uptime : ");debugSerialTX.println(uptime_string_exp());
                  }

                // Handle MQTT callback
                MQTTClient.loop();
                
        } // else (=connected on MQTT server)
        
        ArduinoOTA.handle();                                            // Listen for OTA update
        httpserver.handleClient();                                      // Listen for HTTP requests from clients
        
} // loop
