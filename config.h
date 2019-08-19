#ifndef H_CONFIG
#define H_CONFIG

#include <SoftwareSerial.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

/*********************************************************************************
 * Serial and hardware configuration
/*********************************************************************************/

SoftwareSerial softSerial(4, 2, false,BUFFER_SIZE+2); // software serial RX from GPIO4/D2 pin (unused pin on ESP01), software serial TX to RFLink on GPIO2/D4 pin - uncomment to use software serial
auto& debugSerialTX = Serial;                         // debugSerialTX is to show information for debugging - use Serial to write on hardware serial (ESP TX pin) 
auto& rflinkSerialRX = Serial;                        // rflinkSerialRX is used for data from RFLink - uncomment this line to listen on hardware serial (ESP RX pin)
//auto& rflinkSerialTX = Serial;                      // rflinkSerialTX is used for data to RFLink - uncomment this line and comment following one to write on hardware serial (ESP TX pin)
SoftwareSerial& rflinkSerialTX = softSerial;          // rflinkSerialTX is used for data to RFLink - uncomment this line and comment previous one to write on software serial (GPIO2/D4)

#define MEGA_RESET_PIN 0                      // ESP pin connected to MEGA reset pin - GPIO0 = D3
//#define SERIAL_DEBUG                        // uncomment to enable debug on debugSerialTX

/*********************************************************************************
 * Wi-Fi and MQTT parameters
/*********************************************************************************/

#define WIFI_SSID "XXXXXX"                    // Wi-Fi network SSID for ESP8266 to connect to
#define WIFI_PASSWORD "XXXXXX"                // Wi-Fi password for the network above

#define MQTT_SERVER "192.168.1.201"             // MQTT Server
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

/*********************************************************************************
 * Other parameters
/*********************************************************************************/

#define RESET_MEGA_INTERVAL 0 * 1000 * 60;                  // Auto reset Mega if no data is received (in ms) - 0 to disable
#define MQTT_MEGA_RESETS_TOPIC "rflink/mega_resets"         // Topic for number of times Mega is automatically resetted

/*********************************************************************************
 * Parameters for IDs filtering - this is used to publish on MQTT server only some IDs
/*********************************************************************************/

#define USER_ID_NUMBER 0       // *MUST* be equal to USER_IDs number of lines (following table) OR set to 0 to publish all IDs with no condition

const USER_ID_STRUCT USER_IDs[] = {    // Configure IDs that will be forwarded to MQTT server, interval time to force publish if data did not change (in ms), and description
  { "1082"    , 30 * 1000 * 60 , "Auriol V3 - Thermom&egrave;tre piscine (ID forc&eacute;e)"  },
  { "0210"    , 30 * 1000 * 60 , "Alecto V5 - Pluviom&egrave;tre (ID forc&eacute;e)"  },  
  { "2A1C"    , 30 * 1000 * 60 , "Oregon Rain2 - Pluie"                                       },
};

const USER_SPECIFIC_ID_STRUCT USER_SPECIFIC_IDs[] = {    // This is used to force a specific ID for devices changing frequently ; it applies to a specific name/protocol, which means there should be only one device using this protocol
  //{ "Auriol_V3"  ,  "1082"   },
  //{ "Alecto_V5"  ,  "0210"   },
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
