//********************************************************************************
// RFLink parser functions
//********************************************************************************
#include "Rflink.h"

/**
 * Read RFLink packet (line terminated by \n) and
 * - extract message name / driver (eg: OregonV1) to MQTT_NAME
 * - extract ID of the sender (eg: ID=012345) to MQTT_ID
 * - create a JSON message with all encountered fields
 */
void readRfLinkPacket(char* line) {
        int i = 6; // ignore message type and line number
        int j = 0;
        bool nameHasEq = false;
        bool nameHasDash = false;

        // check len and ignore bad packet (broken serial, etc.)
        if(strlen(line) < RFLINK_PACKET_MIN_SIZE) return;
		
		#ifdef EXPERIMENTAL
			// get line number TEST
			LINE_NUMBER[0] = line[3];
			LINE_NUMBER[1] = line[4];
			LINE_NUMBER[2] = '\0';
                #endif
		
        // get name : 3rd field (begins at char 6)
        while(line[i] != ';' && i < BUFFER_SIZE && j < MAX_DATA_LEN) {
                if      (line[i]==' ') MQTT_NAME[j] = '_';
                else if (line[i]=='=')  { nameHasEq = true; break; }
                else MQTT_NAME[j] = line[i];
		if (line[i]=='-')  { nameHasDash = true; }
                i++; j++;
        }

        // ends string correctly
        MQTT_NAME[j] = '\0';

        // if name contains "=", assumes that it's an rflink message, not an RF packet
        // thus we put a special name and ID=0, then parse to JSON
        if(nameHasEq==true) {
                //Serial.println(F("name contains '=' !"));
                i = 6;
                strcpy_P(MQTT_NAME,PSTR("message"));
                MQTT_ID[0]='0'; MQTT_ID[1]='\0';
                readRfLinkFields(line, i);
                return;
        }

        // if name contains "-", assumes that it's Internal_Pullup_on_RF-in_disabled message (; is missing at the end)
        if(nameHasDash==true) {
                i = 6;
                strcpy_P(MQTT_NAME,PSTR("message"));
                MQTT_ID[0]='0'; MQTT_ID[1]='\0';
                j=0;
                while(line[i] != '\n' && i < BUFFER_SIZE && j < BUFFER_SIZE) {
                        JSON[j++] = line[i++];
                }
                JSON[j-1]='\0';
                return;
        }
	/*	
        if ( strcmp(MQTT_NAME,"STATUS") == 0){
          // Meldung STATUS
          MQTT_ID[0]='6'; MQTT_ID[1]='\0';
          i = 13;
          readRfLinkFields(line, i);
          return;
        }
	*/	
        if (strncmp_P(MQTT_NAME,RFLINK_MQTT_NAME_NODO,12) == 0)
        {
                strcpy_P(MQTT_NAME,PSTR("message"));
                MQTT_ID[0]='0'; MQTT_ID[1]='\0';
                j=0;
                i=6; // Skip ;
                while(line[i] != '\n' && i < BUFFER_SIZE && j < BUFFER_SIZE) {
                                JSON[j++] = line[i++];
                }
                JSON[j-1]='\0';
                return;
        }
        
        // for debug and ACK messages, send them directly, no json convertion
        if(RfLinkIsStringInArray(MQTT_NAME,RFLINK_MQTT_NAMES_NO_JSON)) {
                //Serial.println(F("Special name found => no JSON convertion"));
                strcpy_P(MQTT_NAME,PSTR("message"));
                MQTT_ID[0]='0'; MQTT_ID[1]='\0';
                j=0;
                i=6;
                while(line[i] != '\n' && i < BUFFER_SIZE && j < BUFFER_SIZE) {
                        JSON[j++] = line[i++];
                }
                JSON[j-1]='\0';

                return;
        }


        // for all other messages, get MQTT_ID (4th field) then convert to json
        j=0;
        i+=4; // skip ";MQTT_ID="

        while(line[i] != ';' && i < BUFFER_SIZE && j < MAX_ID_LEN) {
                MQTT_ID[j++] = line[i++];
        }
        MQTT_ID[j] = '\0';

        // continue with json convertion
        readRfLinkFields(line, i+1);
}


/**
 * Extract fields and convert them to a json string
 */
void readRfLinkFields(char* fields, int start){

        int strpos=start;
        int fldpos=0;
        int valueType=0;

        JSON[0]='{';
        JSON[1]='\0';

        char SWITCH_NUMBER[MAX_ID_LEN]; // XXX used to store number in case of SWITCH name
        bool after_SWITCH = 0;
        bool is_SWITCH_CMD = 0;
        
        while(strpos < BUFFER_SIZE-start && fields[strpos] != '\n' && fields[strpos] != '\0') {

                // if current char is "=", we end name parsing and start parsing the field's value
                if(fields[strpos] == '=') {
                        FIELD_BUF[fldpos]='\0';
                        fldpos=0;

                        // Tag field regarding the name...
                        if(RfLinkFieldIsString(FIELD_BUF)) valueType=RFLINK_VALUE_TYPE_STRING;
                        else if(RfLinkFieldIsHexFloat10Neg(FIELD_BUF)) valueType=RFLINK_VALUE_TYPE_FLOAT_NEG;
                        else if(RfLinkFieldIsHexFloat10(FIELD_BUF)) valueType=RFLINK_VALUE_TYPE_FLOAT;
                        else if(RfLinkFieldIsHexInteger(FIELD_BUF)) valueType=RFLINK_VALUE_TYPE_HEX_INTEGER;
                        else if(RfLinkFieldIsDecInteger(FIELD_BUF)) valueType=RFLINK_VALUE_TYPE_DEC_INTEGER;
                        else valueType=RFLINK_VALUE_TYPE_RAWVAL;
                        
                        RfLinkFieldAddQuotedValue(FIELD_BUF);

                        if (strcmp(FIELD_BUF,"CMD") == 0 && after_SWITCH) is_SWITCH_CMD = 1; else is_SWITCH_CMD = 0 ;         // XXX if it is a CMD field after a SWITCH field, we note it
                        if (strcmp(FIELD_BUF,"SWITCH") == 0) after_SWITCH = 1; else after_SWITCH = 0 ;                        // XXX if it is a SWITCH field, we note it

                // if current char is ";", we end parsing value and start parsing another field's name
                } else if(fields[strpos] == ';') {

                        FIELD_BUF[fldpos]='\0';
                        fldpos=0;
                        strcat(JSON,":");

                        // Handle special cases...
                        switch(valueType) {
                        case RFLINK_VALUE_TYPE_STRING:  RfLinkFieldAddQuotedValue(FIELD_BUF); break;
                        case RFLINK_VALUE_TYPE_FLOAT_NEG:  RfLinkFieldAddHexFloat10NegValue(FIELD_BUF); break;
                        case RFLINK_VALUE_TYPE_FLOAT:  RfLinkFieldAddHexFloat10Value(FIELD_BUF); break;
                        case RFLINK_VALUE_TYPE_HEX_INTEGER: RfLinkFieldAddHexIntegerValue(FIELD_BUF); break;
                        case RFLINK_VALUE_TYPE_DEC_INTEGER: RfLinkFieldAddDecIntegerValue(FIELD_BUF); break;
                        default: strcat(JSON,FIELD_BUF);
                        }

                        strcat(JSON,",");

                        if (after_SWITCH) {strcpy(SWITCH_NUMBER,FIELD_BUF);}             // XXX SWITCH field => memorize switch number
                        if (is_SWITCH_CMD) {                                             // XXX CMD field after SWITCH field => add SWITCH with unique number
							strcat(JSON,"\"SWITCH");
							strcat(JSON,SWITCH_NUMBER);
							strcat(JSON,"\":");
							strcat(JSON,"\"");
							strcat(JSON,FIELD_BUF);
							strcat(JSON,"\",");
							is_SWITCH_CMD = 0;
                        } 
                        
                        
                } else { // default case : copy current char
                        FIELD_BUF[fldpos++]=fields[strpos];
                }

                strpos++;
        }

        int len = strlen(JSON);
        JSON[len-1]='}';
}


/**
 * check wether a given string is in a PROGMEN array of strings
 */
bool RfLinkIsStringInArray(char *buffer, const char* const strArray[]) {
        int i = 0;
        char stringptr[40];
        do {
                strcpy_P(stringptr,strArray[i]);
                if (stringptr[0] != '\0') {
                        if(strcmp(buffer, stringptr)==0) return true;
                        i++;
                }
        }
        while(stringptr[0] != '\0');
        return false;
}


/**
 * check if a given field name is used for string (thus need quotes in JSON message)
 */
bool RfLinkFieldIsString(char *buffer) {
        return RfLinkIsStringInArray(buffer, RFLINK_FIELD_STRING);
}


/**
 * check if a given field name is used for hex integer (thus need to be converted to dec)
 */
bool RfLinkFieldIsHexInteger(char *buffer) {
        return RfLinkIsStringInArray(buffer, RFLINK_FIELD_HEXINT);
}

/**
 * check if a given field name is used for decimal integer (thus need to be converted to dec)
 */
bool RfLinkFieldIsDecInteger(char *buffer) {
        return RfLinkIsStringInArray(buffer, RFLINK_FIELD_DECINT);
}

/**
 * check if a given field name needs to be converted to float divided by 10 and possibly negative
 */
bool RfLinkFieldIsHexFloat10Neg(char *buffer) {
    return RfLinkIsStringInArray(buffer, RFLINK_FIELD_HEXFLOAT10_NEG);
}

/**
 * check if a given field name needs to be converted to float divided by 10
 */
bool RfLinkFieldIsHexFloat10(char *buffer) {
    return RfLinkIsStringInArray(buffer, RFLINK_FIELD_HEXFLOAT10);
}


/**
 * put the value as a quoted one in the JSON buffer
 */
void RfLinkFieldAddQuotedValue(char *buffer) {
        strcat(JSON,"\"");
        strcat(JSON,buffer);
        strcat(JSON,"\"");
}


/**
 * convert a string to float value and put it in the JSON buffer ; if is starts by 8, it is a negative value
 * eg : 0x8066 will become -10.2
 */
void RfLinkFieldAddHexFloat10NegValue(char *buffer) {                  	// for TMP, WINCHL and WINTMP
        char strfloat[11];
        if (buffer[0] == 56) {                                          // if first char is a 8 (code ascii 56), it is a negative temp
          buffer[0] = 48;                                               // replace char 8 by char 0 (code ascii 48)
          dtostrf(strtoul(buffer,NULL,16)*0.1*-1, 2, 1, strfloat);      // convert to hex, divide by 10 and multiply by minus 1
        } else {
          dtostrf(strtoul(buffer,NULL,16)*0.1, 2, 1, strfloat);         // convert to hex, divide by 10
        }
        strfloat[10]='\0';
        strcat(JSON,strfloat);
}

/**
 * convert a string to float value and put it in the JSON buffer
 * eg : 0x00c3 which is 216 (dec) will become 21.6
 */
void RfLinkFieldAddHexFloat10Value(char *buffer) {
        char strfloat[11];
        dtostrf(strtoul(buffer,NULL,16)*0.1, 2, 1, strfloat);
        strfloat[10]='\0';
        strcat(JSON,strfloat);
}


/**
 * put the value as an integer in the JSON buffer
 * eg: 0x57 will become 87
 */
void RfLinkFieldAddHexIntegerValue(char *buffer) {
        char s[21];
        strcat(JSON, ultoa(strtoul(buffer,NULL,16),s,10));
}

/**
 * put the value as an integer in the JSON buffer
 * eg: 07 will become 7
 */
void RfLinkFieldAddDecIntegerValue(char *buffer) {
        char s[21];
        strcat(JSON, ultoa(strtoul(buffer,NULL,10),s,10));
}
