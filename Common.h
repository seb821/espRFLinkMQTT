#ifndef H_COMMON
#define H_COMMON

#include <SoftwareSerial.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
//#include <ESP8266mDNS.h>
//#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>         // XXX
#include <ESP8266HTTPUpdateServer.h>  // XXX

/*********************************************************************************
 * Global parameters
/*********************************************************************************/

#define WIFI_SSID "XXXXXX"                		// network SSID for ESP8266 to connect to
#define WIFI_PASSWORD "XXXXXX"                	// password for the network above

#define MQTT_SERVER "192.168.1.101"             // MQTT Server
#define MQTT_PORT 1883                          // MQTT server port
#define MQTT_USER ""                         	// MQTT Server user
#define MQTT_PASSWORD ""                     	// MQTT Server password
#define MQTT_RFLINK_CLIENT_NAME "espRFLinkMQTT" // Client name sent to MQTT server (also used for title, hostname and OTA)

#define MEGA_RESET_PIN 0                        // ESP pin connected to MEGA reset pin - GPIO0 = D3

#define MQTT_PUBLISH_TOPIC "rflink"             // MQTT topic to publish to
#define MQTT_RFLINK_ORDER_TOPIC "rflink/cmd"    // MQTT topic to listen to for rflink order
#define MQTT_WILL_TOPIC "rflink/online"         // MQTT last will topic
#define MQTT_WILL_ONLINE "1"                    // MQTT last will topic value online
#define MQTT_WILL_OFFLINE "0"                   // MQTT last will topic value online

//#define MQTT_DEBUG 0                          // XXX replaced by a variable to enable/disable
#define MQTT_DEBUG_TOPIC "rflink/debug"         // MQTT debug topic to publish raw data from RFLink
#define MQTT_UPTIME_TOPIC "rflink/uptime"       // MQTT topic for uptime

#define BUFFER_SIZE 200                         // Maximum size of one serial line and json content
#define MAX_DATA_LEN 24                         // Maximum size for record name, record id, json field name and json field value
#define MAX_TOPIC_LEN 60                        // Maximum topic path size (at least lenght of MQTT_PUBLISH_TOPIC + 2 x MAX_DATA_LEN)

/*********************************************************************************
 * functions defined in scketch
/*********************************************************************************/
void callback(char* topic, byte* payload, unsigned int length);
void buildMqttTopic(char *name, char *ID);
void printToSerial();

#endif
