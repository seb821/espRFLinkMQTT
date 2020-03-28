#ifndef H_CONFIG
#define H_CONFIG

#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <EEPROM.h>

/*********************************************************************************
 * Serial and hardware configuration
/*********************************************************************************/

auto& debugSerialTX = Serial;                         // debugSerialTX is to show information for debugging - use Serial to write on hardware serial (ESP TX pin) 
auto& rflinkSerialRX = Serial;                        // rflinkSerialRX is used for data from RFLink - uncomment this line to listen on hardware serial (ESP RX pin)
auto& rflinkSerialTX = Serial;                      // rflinkSerialTX is used for data to RFLink
//SoftwareSerial& rflinkSerialTX = softSerial;          // rflinkSerialTX is used for data to RFLink: by default, it uses ESP TX pin ; in order to use another pin, comment previous line and uncomment this line and following ones to write on software serial (GPIO2/D4)
//SoftwareSerial softSerial(4, 2, false); 			// software serial TX to RFLink on GPIO2/D4 ; software serial RX from GPIO4/D2 pin (unused) 
//#include <SoftwareSerial.h>

#define MEGA_RESET_PIN 0                      // ESP pin connected to MEGA reset pin - GPIO0 = D3
//#define SERIAL_DEBUG                        // uncomment to enable debug on debugSerialTX

/*********************************************************************************
 * Wi-Fi and MQTT parameters
/*********************************************************************************/

#define WIFI_SSID "XXXXXX"                    // Wi-Fi network SSID for ESP8266 to connect to
#define WIFI_PASSWORD "XXXXXX"                // Wi-Fi password for the network above

#define MQTT_SERVER "192.168.1.10"             // MQTT Server
#define MQTT_PORT 1883                          // MQTT server port
#define MQTT_USER ""                            // MQTT Server user
#define MQTT_PASSWORD ""                        // MQTT Server password
#define MQTT_RFLINK_CLIENT_NAME "espRFLinkMQTT" // Client name sent to MQTT server (also used for title, hostname and OTA)

#define MQTT_PUBLISH_TOPIC "rflink"             // MQTT topic to publish to
#define MQTT_RFLINK_ORDER_TOPIC "rflink/cmd"    // MQTT topic to listen to for rflink order
#define MQTT_WILL_TOPIC "rflink/online"         // MQTT last will topic ; "rflink/status"
#define MQTT_WILL_ONLINE "1"                    // MQTT last will topic value online; "online"
#define MQTT_WILL_OFFLINE "0"                   // MQTT last will topic value offline; "offline"

#define MQTT_DEBUG_TOPIC "rflink/debug"         // MQTT debug topic to publish raw data, name, ID, MQTT topic and json (json format)
#define MQTT_UPTIME_TOPIC "rflink/uptime"       // MQTT topic for uptime

#define MQTT_MEGA_RESET_TOPIC "rflink/mega_reset"         // MQTT topic whereto publish a 1s pulse before resetting RFLink Mega																																								 
/*********************************************************************************
 * Other parameters
/*********************************************************************************/

#define AUTO_RESET_MEGA_INTERVAL 0 * 1000 * 60;                 // Auto reset Mega if no data is received during this period of time (in ms) - 0 to disable

/*********************************************************************************
 * Parameters for IDs filtering - this is used to publish only some IDs
/*********************************************************************************/

#define USER_ID_NUMBER 32       // If set to 0, there is no ID filtering: everything will be published on the MQTT server. In order to use ID filtering, fill in the following table (USER_IDs) and report the number of configured devices here. Also change VERSION below.

const USER_ID_STRUCT USER_IDs[] = {    // Configure IDs that will be forwarded to MQTT server. Second column is ID used for MQTT topic. Third column is interval time (in ms) to force publication: if data received has not changed within this interval time, it will not be published (note that if data received changed it is always published). Use 0 to publish everytime some data is received. Last column is a description. Please note that filtering on too many IDs may get the ESP to become unstable (tested with 32 devices).
  {"1082","1082",1800000,"Auriol V3"}, //1
  {"0210","0210",1800000,"Alecto V5"}, //2
  {"2A04","2A1C",1800000,"Oregon Rain2"}, //3
  {"00000","00000",1000,"-"}, //4
  {"00000","00000",1000,"-"}, //5
  {"00000","00000",1000,"-"}, //6
  {"00000","00000",1000,"-"}, //7
  {"00000","00000",1000,"-"}, //8
  {"00000","00000",1000,"-"}, //9
  {"00000","00000",1000,"-"}, //10
  {"00000","00000",1000,"-"}, //11
  {"00000","00000",1000,"-"}, //12
  {"00000","00000",1000,"-"}, //13 
  {"00000","00000",1000,"-"}, //14
  {"00000","00000",1000,"-"}, //15
  {"00000","00000",1000,"-"}, //16
  {"00000","00000",1000,"-"}, //17
  {"00000","00000",1000,"-"}, //18
  {"00000","00000",1000,"-"}, //19
  {"00000","00000",1000,"-"}, //20
  {"00000","00000",1000,"-"}, //21
  {"00000","00000",1000,"-"}, //22
  {"00000","00000",1000,"-"}, //23
  {"00000","00000",1000,"-"}, //24
  {"00000","00000",1000,"-"}, //25
  {"00000","00000",1000,"-"}, //26
  {"00000","00000",1000,"-"}, //27
  {"00000","00000",1000,"-"}, //28
  {"00000","00000",1000,"-"}, //29
  {"00000","00000",1000,"-"}, //30
  {"00000","00000",1000,"-"}, //31
  {"00000","00000",1000,"-"}, //32							  
};
// Note: ID filtering configuration is now saved in eeprom memory and can changed online in the /settings page. This is very useful when a device changes ID in order to keep using the same MQTT topic.

#define VERSION 20200328 // ! => Changing this number will overwrite the configuration in eeprom memory (online configuration) with USER_IDs defined above. 
/*
This is how it works :
- at startup software reads version in eeprom memory
- if value is the same as above, it uses eeprom memory configuration
- if it is different, it overwrites eeprom memory configuration with configuration in USER_IDs above
This means that if changes were made online to ID filtering, it is safer to update configuration in USER_IDs above. To make is easy, the /settings will provide the code for that.
Also, if USER_ID_NUMBER is changed, it is safer to change VERSION as well 
*/											

const USER_SPECIFIC_ID_STRUCT USER_SPECIFIC_IDs[] = {    // This is used to force a specific ID for devices changing frequently ; it applies to a specific name/protocol, which means there should be only one device using this protocol
  //{ "Auriol_V3"  ,  "ALL"  , "1082"   },
  //{ "Alecto_V5"  ,  "0001" , "0210"   },
};

const USER_CMD_STRUCT USER_CMDs[] = {    // Configure commands to RFLink to show on web interface
  { "Ping"       ,  "10;ping;"                              },
  { "Version"    ,  "10;version;"                           },
  { "Status"     ,  "10;status;"                            },
  { "Reboot"     ,  "10;reboot;"                            },
};

/*********************************************************************************
 * functions defined in scketch
/*********************************************************************************/
void callback(char* topic, byte* payload, unsigned int length);
void buildMqttTopic(char *name, char *ID);
void printToSerial();

#endif
