#ifndef H_RFLINK
#define H_RFLINK

#define BUFFER_SIZE 200                         // Maximum size of one serial line and json content
#define MAX_ID_LEN 10                         	// Maximum size for record id
#define MAX_DATA_LEN 24                         // Maximum size for record name, json field name and json field value
#define MAX_TOPIC_LEN 60                        // Maximum topic path size (at least lenght of MQTT_PUBLISH_TOPIC + 2 x MAX_DATA_LEN)

#include <Arduino.h>

//********************************************************************************
// RFLink settings
//********************************************************************************

// if less than that, rflink line will be ignored
#define RFLINK_PACKET_MIN_SIZE 8

// Rflink value type tags
#define RFLINK_VALUE_TYPE_HEX_INTEGER     	0
#define RFLINK_VALUE_TYPE_STRING      		1
#define RFLINK_VALUE_TYPE_FLOAT_NEG   		2
#define RFLINK_VALUE_TYPE_FLOAT       		3
#define RFLINK_VALUE_TYPE_RAWVAL      		4
#define RFLINK_VALUE_TYPE_DEC_INTEGER     	5


// main input / output buffers
//extern char BUFFER [BUFFER_SIZE];
extern char JSON   [BUFFER_SIZE];

// message builder buffers
extern char MQTT_NAME[MAX_DATA_LEN];
extern char MQTT_ID  [MAX_ID_LEN+1];
extern char FIELD_BUF[MAX_DATA_LEN];
#ifdef EXPERIMENTAL
	extern char LINE_NUMBER[3]; // TEST
#endif

//********************************************************************************
// const strings used in helper functions
//********************************************************************************
// list of fields that must be quoted in JSON convertion
const char PROGMEM RFLINK_FIELD_NAME_CMD[]         = "CMD";
const char PROGMEM RFLINK_FIELD_NAME_BAT[]         = "BAT";
const char PROGMEM RFLINK_FIELD_NAME_CHIME[]       = "CHIME";
const char PROGMEM RFLINK_FIELD_NAME_SMOKEALERT[]  = "SMOKEALERT";
const char PROGMEM RFLINK_FIELD_NAME_SWITCH[]      = "SWITCH";
const char PROGMEM RFLINK_FIELD_NAME_PIR[]         = "PIR";
const char PROGMEM RFLINK_FIELD_NAME_SET_LEVEL[]   = "SET_LEVEL";
const char PROGMEM RFLINK_FIELD_NAME_RFDEBUG[]     = "RFDEBUG";
const char* const PROGMEM RFLINK_FIELD_STRING[] = {
        RFLINK_FIELD_NAME_CMD,
        RFLINK_FIELD_NAME_BAT,
        RFLINK_FIELD_NAME_CHIME,
        RFLINK_FIELD_NAME_SMOKEALERT,
        RFLINK_FIELD_NAME_SWITCH,
        RFLINK_FIELD_NAME_PIR,
        RFLINK_FIELD_NAME_SET_LEVEL,
        RFLINK_FIELD_NAME_RFDEBUG,
        "\0" // do not remove this mark the end of the array
};


// list of fields with hex encoded integer that must be converted to decimal integer
const char PROGMEM RFLINK_FIELD_NAME_WATT[]        = "WATT";
const char PROGMEM RFLINK_FIELD_NAME_KWATT[]       = "KWATT";
const char PROGMEM RFLINK_FIELD_NAME_BARO[]        = "BARO";
const char PROGMEM RFLINK_FIELD_NAME_UV[]          = "UV";
const char PROGMEM RFLINK_FIELD_NAME_LUX[]         = "LUX";
const char* const PROGMEM RFLINK_FIELD_HEXINT[] = {
        RFLINK_FIELD_NAME_WATT,
        RFLINK_FIELD_NAME_KWATT,
        RFLINK_FIELD_NAME_BARO,
        RFLINK_FIELD_NAME_UV,
        RFLINK_FIELD_NAME_LUX,
        "\0" // do not remove this mark the end of the array
};

// list of fields with integer that must be converted to decimal integer
const char PROGMEM RFLINK_FIELD_NAME_HUM[]        = "HUM";
const char* const PROGMEM RFLINK_FIELD_DECINT[] = {
        RFLINK_FIELD_NAME_HUM,
        "\0" // do not remove this mark the end of the array
};

// list of detected MQTT names that implies no json convertion, but a direct copy of the buffer
const char PROGMEM RFLINK_MQTT_NAME_DEBUG[]      = "DEBUG";
const char PROGMEM RFLINK_MQTT_NAME_Debug[]      = "Debug";
const char PROGMEM RFLINK_MQTT_NAME_OK[]         = "OK";
const char PROGMEM RFLINK_MQTT_NAME_CMD_UNKNOWN[]= "CMD_UNKNOWN";
const char PROGMEM RFLINK_MQTT_NAME_PONG[]       = "PONG";
const char PROGMEM RFLINK_MQTT_NAME_BLE_DEBUG[]  = "BLE_DEBUG";
const char PROGMEM RFLINK_MQTT_NAME_STATUS[]     = "STATUS";
const char PROGMEM RFLINK_MQTT_NAME_NODO[]       = "Nodo_RadioFrequencyLink";
const char PROGMEM RFLINK_MQTT_NAME_PULLUP[]     = "Internal_Pullup_on_RF-in_disabled";
const char* const PROGMEM RFLINK_MQTT_NAMES_NO_JSON[] = {
        RFLINK_MQTT_NAME_DEBUG,
        RFLINK_MQTT_NAME_Debug,
        RFLINK_MQTT_NAME_OK,
        RFLINK_MQTT_NAME_BLE_DEBUG,
        RFLINK_MQTT_NAME_STATUS,
        RFLINK_MQTT_NAME_CMD_UNKNOWN,
        RFLINK_MQTT_NAME_PONG,
        RFLINK_MQTT_NAME_PULLUP,
        "\0" // do not remove this mark the end of the array
};

// list of fields with hex encoded integer that must be converted to decimal integer divided by 10 and possibly negative
const char PROGMEM RFLINK_MQTT_NAME_TEMP[]      = "TEMP";
const char PROGMEM RFLINK_MQTT_NAME_WINCHL[]    = "WINCHL";
const char PROGMEM RFLINK_MQTT_NAME_WINTMP[]    = "WINTMP";
const char* const PROGMEM RFLINK_FIELD_HEXFLOAT10_NEG[] = {
        RFLINK_MQTT_NAME_TEMP,
        RFLINK_MQTT_NAME_WINCHL,
        RFLINK_MQTT_NAME_WINTMP,
        "\0" // do not remove this mark the end of the array
};

// list of fields with hex encoded integer that must be converted to decimal integer divided by 10

const char PROGMEM RFLINK_MQTT_NAME_RAIN[]      = "RAIN";
const char PROGMEM RFLINK_MQTT_NAME_RAINRATE[]  = "RAINRATE";
const char PROGMEM RFLINK_MQTT_NAME_RAINTOT[]   = "RAINTOT";
const char PROGMEM RFLINK_MQTT_NAME_WINSP[]     = "WINSP";
const char PROGMEM RFLINK_MQTT_NAME_WINGS[]     = "WINGS";
const char PROGMEM RFLINK_MQTT_NAME_AWINSP[]    = "AWINSP";
const char* const PROGMEM RFLINK_FIELD_HEXFLOAT10[] = {
        RFLINK_MQTT_NAME_RAIN,
        RFLINK_MQTT_NAME_RAINRATE,
        RFLINK_MQTT_NAME_RAINTOT,
        RFLINK_MQTT_NAME_WINSP,
        RFLINK_MQTT_NAME_WINGS,
        RFLINK_MQTT_NAME_AWINSP,
        "\0" // do not remove this mark the end of the array
};


//********************************************************************************
//* RFLink functions
//********************************************************************************
void readRfLinkPacket(char* line);
void readRfLinkFields(char* fields, int start);

bool RfLinkFieldIsString(char *buffer);
bool RfLinkFieldIsHexFloat10Neg(char *buffer);
bool RfLinkFieldIsHexFloat10(char *buffer);
bool RfLinkFieldIsHexInteger(char *buffer);
bool RfLinkFieldIsDecInteger(char *buffer);

void RfLinkFieldAddQuotedValue (char *buffer);
void RfLinkFieldAddHexFloat10NegValue (char *buffer);
void RfLinkFieldAddHexFloat10Value (char *buffer);
void RfLinkFieldAddHexIntegerValue(char *buffer);
void RfLinkFieldAddDecIntegerValue(char *buffer);

// bool RfLinkIsStringInArray(char *buffer, char* strArray[]);
bool RfLinkIsStringInArray(char *buffer, const char* const strArray[]);

#endif
