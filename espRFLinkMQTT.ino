
// Configuration is to be done in config.h file

// Global project file
#include "espRFLinkMQTT.h"


//********************************************************************************
// Declare Wifi and MQTT
//********************************************************************************

WiFiClient wifiClient;

PubSubClient MQTTClient;

//PubSubClient MQTTClient(eepromConfig.mqtt.server, eepromConfig.mqtt.port, callback, wifiClient);

ESP8266WebServer httpserver(80);                               // Create a webserver object that listens for HTTP request on port 80

ESP8266HTTPUpdateServer httpUpdater;                           // Firmware webupdate

#ifdef USE_AUTOCONNECT
	AutoConnectAux  update("/update", "Update firmware");
	AutoConnectAux  mqtt("/mqtt_setting", "MQTT setting");
	ACText(header, "MQTT broker settings");
	ACText(caption, "Enter here MQTT settings");
	AutoConnect	portal(httpserver); // Autoconnect
	AutoConnectConfig AC_config; // Autoconnect
#endif

//********************************************************************************
// Functions
//********************************************************************************

/** Serial debug functions */
#if defined(SERIAL_DEBUG) 
	#define DEBUG_PRINT(x)   	debugSerialTX.print(x);
	#define DEBUG_PRINTF(x,y)  	debugSerialTX.printf(x,y);
	#define DEBUG_PRINTLN(x) 	debugSerialTX.println(x);
	#define DEBUG_WRITE(x,y)   	debugSerialTX.write(x,y);
#else
	#define DEBUG_PRINT(x)   	{};
	#define DEBUG_PRINTF(x,y)  	{};
	#define DEBUG_PRINTLN(x) 	{};
	#define DEBUG_WRITE(x, y) 	{};
#endif

/** Save in EEPROM memory current eepromConfig */
void saveEEPROM() {
  EEPROM.begin(4096);
  // Update version number before writing eeprom
  eepromConfig.version = CONFIG_VERSION;
  EEPROM.put(eepromAddress, eepromConfig);
  EEPROM.commit();
  EEPROM.end();
  DEBUG_PRINTLN("Config saved to EEPROM");
}

/** Load eepromConfig from EEPROM memory */
void loadEEPROM() {
  EEPROM.begin(4096);
  DEBUG_PRINT("EEPROM size: ");DEBUG_PRINTLN(EEPROM.length());
  DEBUG_PRINT("EEPROM configuration length: ");DEBUG_PRINTLN(eepromLength);
  EEPROM.get(eepromAddress,eepromConfig);  
  EEPROM.end();
}

/** Show eepromConfig from EEPROM memory */
void showEEPROM() {
		DEBUG_PRINTLN("EEPROM content:");
		DEBUG_PRINTLN(" - MQTT");
		DEBUG_PRINT(" | MQTT server: ");DEBUG_PRINTLN(eepromConfig.mqtt.server);
		DEBUG_PRINT(" | MQTT port: ");DEBUG_PRINTLN(eepromConfig.mqtt.port);
		DEBUG_PRINT(" | MQTT user: ");DEBUG_PRINTLN(eepromConfig.mqtt.user);
		DEBUG_PRINT(" | MQTT password: ");DEBUG_PRINTLN(eepromConfig.mqtt.password);
		DEBUG_PRINT(" - ID filtering ");DEBUG_PRINTLN(eepromConfig.id_filtering);
		for (int i =0; i < FILTERED_ID_SIZE; i++) {
			DEBUG_PRINT(" | ");DEBUG_PRINT(eepromConfig.filtered_id[i].id);
			DEBUG_PRINT(" \t\t- ");DEBUG_PRINT(eepromConfig.filtered_id[i].id_applied);  
			DEBUG_PRINT(" \t\t- ");DEBUG_PRINT(eepromConfig.filtered_id[i].publish_interval);
			DEBUG_PRINT(" \t\t- ");DEBUG_PRINT(eepromConfig.filtered_id[i].description);
			DEBUG_PRINTLN();
		}
		DEBUG_PRINT(" - Version ");DEBUG_PRINTLN(eepromConfig.version);
};

/** Check config from EEPROM memory */
void checkEEPROM() {
	
	
	/*DEBUG_PRINTLN("Checking MQTT configuration");
	#ifdef ENABLE_MQTT_SETTINGS_ONLINE_CHANGE
		// Update MQTT settings from EEPROM
		if (strcmp(eepromConfig.mqtt.server,"")==0) {
			strncpy(eepromConfig.mqtt.server,MQTT_SERVER,CFG_MQTT_SERVER_SIZE);
			DEBUG_PRINT(F("MQTT server default value used: "));DEBUG_PRINTLN(eepromConfig.mqtt.server);
		}
		if (!eepromConfig.mqtt.port) {
			eepromConfig.mqtt.port = MQTT_PORT;
			DEBUG_PRINT(F("MQTT port default value used: "));DEBUG_PRINTLN(eepromConfig.mqtt.port);
		}
	#else
	#endif*/
    
	
	// Check EEPROM version configuration
	DEBUG_PRINT("Config version in EEPROM:    ")DEBUG_PRINTLN(eepromConfig.version);
	DEBUG_PRINT("Config version in config.h:  ")DEBUG_PRINTLN(CONFIG_VERSION);
	bool version_changed = eepromConfig.version != CONFIG_VERSION;
	if (version_changed) {
		DEBUG_PRINTLN("Config version changed => initialize EEPROM configuration from firmware (config.h)");
		// Use ID filtering configuration from config.h
		eepromConfig.id_filtering = id_filtering;
		if (1) {
			for (int i =0; i < filtered_id_number; i++) {
			  strcpy(eepromConfig.filtered_id[i].id,filtered_IDs[i].id);
			  strcpy(eepromConfig.filtered_id[i].id_applied,filtered_IDs[i].id_applied);
			  eepromConfig.filtered_id[i].publish_interval = filtered_IDs[i].publish_interval;
			  strcpy(eepromConfig.filtered_id[i].description,filtered_IDs[i].description);
			}
		}
		// Use MQTT settings from config.h
		strncpy(eepromConfig.mqtt.server,MQTT_SERVER,CFG_MQTT_SERVER_SIZE);
		eepromConfig.mqtt.port = MQTT_PORT;
		strncpy(eepromConfig.mqtt.user,MQTT_USER,CFG_MQTT_USER_SIZE);
		strncpy(eepromConfig.mqtt.password,MQTT_PASSWORD,CFG_MQTT_PASSWORD_SIZE);
		// Save EEPROM
		saveEEPROM();
		loadEEPROM();
	}
	else DEBUG_PRINTLN("Version did not change - using current EEPROM configuration");
		
	showEEPROM();
}

	

#ifdef USE_AUTOCONNECT
	void setup_wifi_autoconnect() {

			DEBUG_PRINTLN("Starting WiFi with Autoconnect");
			// AutConnect parameters
				AC_config.autoReconnect = false;					// If true and wifi is disconnected, uses configuration stored in eeprom to reconnect 
				AC_config.autoSave = AC_SAVECREDENTIAL_NEVER;		// Disables saving of credentials in eeprom (but not esp WiFi config (SDK))
				AC_config.portalTimeout = 600000; 				// Access point timeout after 10 minutes
				AC_config.title = CLIENT_NAME; 					// Menu title
				AC_config.apid = CLIENT_NAME; 					// Access Point name - + String("-") + String(ESP.getChipId(), HEX); 
				AC_config.psk  = ""; 							// Access Point password
				AC_config.hostName = CLIENT_NAME; 	// Hostname
				AC_config.boundaryOffset = eepromLength; 		// Offset to avoid overwriting IP filtering configuration
			portal.config(AC_config);
			// List Autoconnect stored wifi credentials
				AutoConnectCredential credential(AC_config.boundaryOffset);
				station_config_t entry;
				uint8_t count = credential.entries();
				DEBUG_PRINT("AutoConnect Wifi credentials: ");DEBUG_PRINT(count);DEBUG_PRINTLN(" entrie(s)");	
				for (int8_t i = 0; i < count; i++) {    // Loads all entries.
					credential.load(i, &entry);
					DEBUG_PRINT(" | credential ");DEBUG_PRINT(i);DEBUG_PRINT(" - SSID: ");DEBUG_PRINTLN((char *) &entry.ssid);
				}
			// Starts AutoConnect portal
			if (portal.begin()) {
				DEBUG_PRINTLN("WiFi connected to " + WiFi.SSID() + " with Autoconnect, IP address:\t" + WiFi.localIP().toString());
			} else {
				DEBUG_PRINTLN("Connection failed.");
				while (true) {yield();}
			}
	}
	
	void deleteAllCredentials(void) {
		AutoConnectCredential credential(AC_config.boundaryOffset);
		station_config_t config;
		uint8_t ent = credential.entries();
		DEBUG_PRINT("deleteAllCredentials - ");DEBUG_PRINT(ent);DEBUG_PRINTLN(" entrie(s)");		
		while (ent--) {
		credential.load((int8_t)0, &config);
		DEBUG_PRINT("Erase Wifi configuration for ");DEBUG_PRINTLN((char *) &config.ssid);
		credential.del((const char*)&config.ssid[0]);
		}
	}

#elif USE_WM

void setup_wifi_wm() {
	
  pinMode(PIN_LED, OUTPUT);
  Serial.println("\nStarting WifiManager");
  unsigned long startedAt = millis();
  digitalWrite(PIN_LED, LED_ON); // turn the LED on by making the voltage LOW to tell us we are in configuration mode.
  
  //Local intialization. Once its business is done, there is no need to keep it around
  // Use this to default DHCP hostname to ESP8266-XXXXXX or ESP32-XXXXXX
  ESP_WiFiManager ESP_wifiManager;
  // Use this to personalize DHCP hostname (RFC952 conformed)
  //ESP_WiFiManager ESP_wifiManager("ConfigOnStartup");
  
  ESP_wifiManager.setMinimumSignalQuality(-1);
  // Set static IP, Gateway, Subnetmask, DNS1 and DNS2. New in v1.0.5
  // ESP_wifiManager.setSTAStaticIPConfig(IPAddress(192,168,2,114), IPAddress(192,168,2,1), IPAddress(255,255,255,0), 
  //                                      IPAddress(192,168,2,1), IPAddress(8,8,8,8));

  // We can't use WiFi.SSID() in ESP32as it's only valid after connected. 
  // SSID and Password stored in ESP32 wifi_ap_record_t and wifi_config_t are also cleared in reboot
  // Have to create a new function to store in EEPROM/SPIFFS for this purpose
  Router_SSID = ESP_wifiManager.WiFi_SSID();
  Router_Pass = ESP_wifiManager.WiFi_Pass();
  
  //Remove this line if you do not want to see WiFi password printed
  Serial.println("Stored: SSID = " + Router_SSID + ", Pass = " + Router_Pass);
  
  //Check if there is stored WiFi router/password credentials.
  //If not found, device will remain in configuration mode until switched off via webserver.
  Serial.print("Opening configuration portal.");
  
  if (Router_SSID != "")
  {
    ESP_wifiManager.setConfigPortalTimeout(60); //If no access point name has been previously entered disable timeout.
    Serial.println("Timeout 60s");
  }
  else
    Serial.println("No timeout");

  // SSID to uppercase 
  ssid.toUpperCase();  

  //it starts an access point 
  //and goes into a blocking loop awaiting configuration
  if (!ESP_wifiManager.startConfigPortal((const char *) ssid.c_str(), password)) 
    Serial.println("Not connected to WiFi but continuing anyway.");
  else 
    Serial.println("WiFi connected...yeey :)");

  digitalWrite(PIN_LED, LED_OFF); // Turn led off as we are not in configuration mode.
 
  // For some unknown reason webserver can only be started once per boot up 
  // so webserver can not be used again in the sketch.
  #define WIFI_CONNECT_TIMEOUT        30000L
  #define WHILE_LOOP_DELAY            200L
  #define WHILE_LOOP_STEPS            (WIFI_CONNECT_TIMEOUT / ( 3 * WHILE_LOOP_DELAY ))
  
  startedAt = millis();
  
  while ( (WiFi.status() != WL_CONNECTED) && (millis() - startedAt < WIFI_CONNECT_TIMEOUT ) )
  {   
    int i = 0;
    while((!WiFi.status() || WiFi.status() >= WL_DISCONNECTED) && i++ < WHILE_LOOP_STEPS)
    {
      delay(WHILE_LOOP_DELAY);
    }    
  }

  Serial.print("After waiting ");
  Serial.print((millis()- startedAt) / 1000);
  Serial.print(" secs more in setup(), connection result is ");

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print("connected. Local IP: ");
    Serial.println(WiFi.localIP());
  }

}


#else
	void setup_wifi() {
			delay(10);
			DEBUG_PRINT("Starting WiFi, connecting to ");
			DEBUG_PRINT(WIFI_SSID); DEBUG_PRINTLN(" ...");
			WiFi.hostname(CLIENT_NAME);
			WiFi.mode(WIFI_STA); 											// Act as wifi_client only, defaults to act as both a wifi_client and an access-point.
			WiFi.begin(WIFI_SSID,WIFI_PASSWORD);                            // Connect to the network
			int i; i = 0;
			while (WiFi.status() != WL_CONNECTED) {                         // Wait for the Wi-Fi to connect
			  delay(1000);
			  DEBUG_PRINT(i++); DEBUG_PRINT(' ');
			}
			DEBUG_PRINTLN();
			DEBUG_PRINTLN("WiFi connected to " + WiFi.SSID() + ", IP address:\t" + WiFi.localIP().toString());
	}
#endif

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

		mqttConnectionAttempts++;DEBUG_PRINT(F("MQTT connection attempts: "));DEBUG_PRINTLN(mqttConnectionAttempts);
        // connect to Mqtt server and subcribe to order topic
		char uniqueId[6];
		uniqueId[0] = '-';
		for(int i = 1; i<=4; i++){uniqueId[i]= 0x30 | random(0,10);}
		uniqueId[5] = '\0';
		char clientId[MAX_TOPIC_LEN] = "";
		strcpy(clientId, CLIENT_NAME);
		strcat(clientId, uniqueId);
		DEBUG_PRINT("MQTT clienId: ");DEBUG_PRINTLN(clientId);
        if (MQTTClient.connect(clientId, eepromConfig.mqtt.user, eepromConfig.mqtt.password, MQTT_WILL_TOPIC, 0, 1, MQTT_WILL_OFFLINE)) {
                MQTTClient.subscribe(MQTT_RFLINK_CMD_TOPIC);
                MQTTClient.publish(MQTT_WILL_TOPIC,MQTT_WILL_ONLINE,1);   // once connected, update status of will topic
        }
        // report mqtt connection status
        DEBUG_PRINT(F("MQTT connection state: "));DEBUG_PRINTLN(MQTTClient.state());
        return MQTTClient.connected();
		
}

/**
 * OTA
 */
 

void SetupOTA() {

  // ArduinoOTA.setPort(8266);                                      // Port defaults to 8266
  ArduinoOTA.setHostname(CLIENT_NAME);                  // Hostname defaults to esp8266-[ChipID]
  
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
  


void ConfigHTTPserver() {

  httpserver.on("/esp.css",[](){       // CSS
	DEBUG_PRINTLN("Html page requested: /esp.css");
	httpserver.sendHeader("Access-Control-Max-Age", "86400");
	String cssMessage = FPSTR(cssDatasheet);
	httpserver.send(200, "text/css", cssMessage);
	
  });
	
  httpserver.on("/",[](){       // Home page
	DEBUG_PRINTLN("Html page requested: /");

	// URL arguments
	int page = 0;
	String arg_page = httpserver.arg("page");
	if (arg_page.length() > 0) {
		page = max((int) arg_page.toInt(),0);
	};
	
	/*
    // Head
    htmlMessage += "<head><title>" + String(CLIENT_NAME) + "</title>\r\n";
    htmlMessage += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>\r\n";
    htmlMessage += "<meta http-equiv='Content-Type' content='text/html; charset=UTF-8' />\r\n";
	htmlMessage += "<link rel=\"stylesheet\" type=\"text/css\" href=\"/esp.css\">\r\n";
	//htmlMessage += "<style type='text/css'>" + String(FPSTR(cssDatasheet)) + "</style>\r\n";
    htmlMessage += "</head>\r\n";

    // Body
    htmlMessage += "<body class='bodymenu'>\r\n";
	*/
	
	String htmlMessage = "";
	// Page start
	htmlMessage += FPSTR(htmlStart);
	
	
	// Header + menu
	htmlMessage += FPSTR(htmlMenu);
	htmlMessage += "<script>document.getElementById('menuhome').classList.add('active');</script>";
	
    /*
	// Header + menu
    htmlMessage += "<header class='headermenu'>";
    htmlMessage += "<h1>" + String(CLIENT_NAME) + "</h1>\r\n";
    htmlMessage += "<div class='menubar'><a class='menu active' href='.'>&#8962;<span class='showmenulabel'>Home</span></a>\r\n";
    htmlMessage += "<a class='menu' href='/livedata'>&#10740;<span class='showmenulabel'>Live Data</span></a>\r\n";
    htmlMessage += "<a id='resetmega' class='menu' href='/reset-mega' onclick = \"fetch('/reset-mega');return false;\">&#128204;<span class='showmenulabel'>Reset MEGA</span></a>\r\n";
    htmlMessage += "<a class='menu' id='reboot' href='/reboot'>&#128268;<span class='showmenulabel'>Reboot ESP</span></a>\r\n";
    //htmlMessage += (!MQTT_DEBUG)? "<a class='menu' href='/enable-debug'>&#128172;<span class='showmenulabel'>Enable MQTT debug</span></a>\r\n":"<a class='menu' href='/disable-debug'>&#128172;<span class='showmenulabel'>Disable MQTT debug</span></a>\r\n";
    htmlMessage += "<a class='menu' href='/infos'>&#128295;<span class='showmenulabel'>System Info</span></a></div>\r\n";
    htmlMessage += "</header>\r\n";	*/
	
	
	htmlMessage += "<script>var menuactive = document.getElementById('menuhome');  menuactive.classList.add('active');</script>";
    
    // Live data
    htmlMessage += "<h3>RFLink Live Data *</h3>\r\n";

    htmlMessage += "<input type=\"button\" value =\"Pause\" onclick=\"stopUpdate();\" />";                                                                // Pause
    htmlMessage += "<input type=\"button\" value =\"Restart\" onclick=\"restartUpdate();\" />";                                                           // Restart
    htmlMessage += "<input type=\"button\" value =\"Refresh\" onclick=\"window.location.reload(true);\" />\r\n";                                          // Refresh
    
    htmlMessage += "<table id=\"liveData\" class='multirow multirow-left';>\r\n";                                                                         // Table of x lines
    htmlMessage += "<tr class=\"header\"><th class='t-left'>Raw Data</th><th class='t-left'> MQTT Topic </th><th class='t-left'> MQTT JSON </th></tr>\r\n";
    for (int i = 0; i < (7); i++){ // not too high to avoid overflow
      htmlMessage += "<tr id=\"data" + String(i) + "\"><td></td><td></td><td></td></tr>\r\n";
    } 
    htmlMessage += "</table>\r\n";
    
    htmlMessage += "<script>\r\n";                                                        // Script to update data and move to next line
    htmlMessage += "var x = setInterval(function() {loadData(\"/data.txt\",updateData)}, 500);\r\n";       // update every 500 ms
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
    htmlMessage += " x = setInterval(function() {loadData(\"/data.txt\",updateData)}, 500);\r\n";       // update every 500 ms
    htmlMessage += "}\r\n";
    htmlMessage += "</script>\r\n";
    htmlMessage += "<div class='note'>* see Live \"Data tab\" for more lines - web view may not catch all frames, MQTT debug is more accurate</div>\r\n";
    
    // Commands to RFLink
    htmlMessage += "<h3>Commands to RFLink</h3><br />";
    htmlMessage += "<form action=\"/send\" id=\"form_command\" style=\"float: left;\"><input type=\"text\" size=\"32\" id=\"command\" name=\"command\">";
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
    for (int i = 0; i < (int) (sizeof(user_cmds) / sizeof(user_cmds[0])); i++){      // User commands defined in user_cmds for quick access
      htmlMessage += "<a class='button link' style=\"float: left;\" href=\"javascript:void(0)\" "; 
      htmlMessage += "onclick=\"document.getElementById('command').value = '" + String(user_cmds[i][1]) + "';";
      htmlMessage += "submitForm(); return false;\">" + String(user_cmds[i][0]) + "</a>\r\n";  
	}
    htmlMessage += "<br style=\"clear: both;\"/>\r\n";
	
    // Filtered IDs
	htmlMessage += "<a id=\"userids\"/></a><br>\r\n";
    if (eepromConfig.id_filtering) {                                                                         // show list of user IDs
		htmlMessage += "<h3 id='configuration'>Filtered IDs * </h3><br>\r\n";
		if (filtered_id_number > 16) {
			for (int i = 0; i <= ((int) (filtered_id_number-1)/16); i++){
			   htmlMessage += "<a href='/?page=" + String(i) + "#userids' class='button link'>" + String(i*16+1) + " - " + String(min(filtered_id_number,i*16+16)) + "</a>\r\n";
			}
		htmlMessage += "<a class='button link' href='/configuration'>Configure</a><br><br>\r\n";
		}
      htmlMessage += "<table class='multirow'><tr><th>#</th><th>ID</th><th>ID applied</th><th>Description</th><th>Interval</th><th>Received</th><th>Published</th><th class='t-left'>Last MQTT JSON</th></tr>\r\n";
      //for (i = 0; i < (filtered_id_number); i++){
	  for (int i = 0+(16*page); i < min(filtered_id_number,16*(page+1)); i++){
        if (strcmp(eepromConfig.filtered_id[i].id,"") != 0) {
          htmlMessage += "<tr><td>" + String(i+1) + "</td><td>" + String(eepromConfig.filtered_id[i].id) + "</td>";
          htmlMessage += "<td>" + String(eepromConfig.filtered_id[i].id_applied) + "</td>";
          htmlMessage += "<td class='t-left'>" + String(eepromConfig.filtered_id[i].description) + "</td>";
          htmlMessage += "<td>" + String( int(float(eepromConfig.filtered_id[i].publish_interval) *0.001 /60)) + " min</td>";
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
		  #ifdef LOAD_TEST
			htmlMessage += "<td class='t-left'>123456789012345678901234567890123456789012345678901234567890123456789012</td></tr>\r\n";
		  #else
			htmlMessage += "<td class='t-left'>" + String(matrix[i].json) + "</td></tr>\r\n";
		  #endif
        }
      }
      htmlMessage += "</table>\r\n";
      htmlMessage += "<div class='note'>* only those IDs are published on MQTT server, and only if data changed or interval time is exceeded</div>\r\n";
    } else {
		htmlMessage += "<h3 id='configuration'>No filtered IDs * </h3>\r\n";
		htmlMessage += "<div class='note'>* all IDs are forwarded to MQTT server</div>\r\n";
    }

	// Page end
	htmlMessage += FPSTR(htmlEnd);
	
	DEBUG_PRINT("Free mem: ");DEBUG_PRINTLN(ESP.getFreeHeap());
    httpserver.send(200, "text/html", htmlMessage);
  }); // server.on("/"...



  httpserver.on("/livedata",[](){       // 
	DEBUG_PRINTLN("Html page requested: /live-data");
	
    /*String htmlMessage = "<!DOCTYPE html>\r\n<html>\r\n";

    // Head
    htmlMessage += "<head><title>" + String(CLIENT_NAME) + "</title>\r\n";
    htmlMessage += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>\r\n";
	htmlMessage += "<link rel=\"stylesheet\" type=\"text/css\" href=\"/esp.css\">\r\n";
	//htmlMessage += "<style type='text/css'>" + String(FPSTR(cssDatasheet)) + "</style>\r\n";
    htmlMessage += "</head>\r\n";

    // Body
    htmlMessage += "<body class='bodymenu'>\r\n";

    
	// Header + menu
    htmlMessage += "<header class='headermenu'>";
    htmlMessage += "<h1>" + String(CLIENT_NAME) + "</h1>\r\n";
    htmlMessage += "<div class='menubar'><a class='menu' href='.'>&#8962;<span class='showmenulabel'>Home</span></a>\r\n";
    htmlMessage += "<a class='menu active' href='/livedata'>&#10740;<span class='showmenulabel'>Live Data</span></a>\r\n";
    htmlMessage += "<a id='resetmega' class='menu' href='/reset-mega' onclick = \"fetch('/reset-mega');return false;\">&#128204;<span class='showmenulabel'>Reset MEGA</span></a>\r\n";
    htmlMessage += "<a class='menu' id='reboot' href='/reboot'>&#128268;<span class='showmenulabel'>Reboot ESP</span></a>\r\n";
    //htmlMessage += (!MQTT_DEBUG)? "<a class='menu' href='/enable-debug'>&#128172;<span class='showmenulabel'>Enable MQTT debug</span></a>\r\n":"<a class='menu' href='/disable-debug'>&#128172;<span class='showmenulabel'>Disable MQTT debug</span></a>\r\n";
    htmlMessage += "<a class='menu' href='/infos'>&#128295;<span class='showmenulabel'>System Info</span></a></div>\r\n";
    htmlMessage += "</header>\r\n";*/
	
	String htmlMessage = "";
	// Page start
	htmlMessage += FPSTR(htmlStart);
	
	// Header + menu
	htmlMessage += FPSTR(htmlMenu);
	htmlMessage += "<script>document.getElementById('menulivedata').classList.add('active');</script>";
    
    // Live data
    htmlMessage += "<h3>RFLink Live Data</h3>\r\n";

    htmlMessage += "<input type=\"button\" value =\"Pause\" onclick=\"stopUpdate();\" />";                                                                // Pause
    htmlMessage += "<input type=\"button\" value =\"Restart\" onclick=\"restartUpdate();\" />";                                                           // Restart
    htmlMessage += "<input type=\"button\" value =\"Refresh\" onclick=\"window.location.reload(true);\" />\r\n";                                          // Refresh                                                                                   // OK
    htmlMessage += "<input type=\"text\" id=\"mySearch\" onkeyup=\"filterLines()\" placeholder=\"Search for...\" title=\"Type in a name\"><br />\r\n";    // Search
    
    htmlMessage += "<table id=\"liveData\" class='multirow multirow-left';>\r\n";                                                                         // Table of x lines
    htmlMessage += "<tr class=\"header\"><th class='t-left'> <a onclick='sortTable(0)'>Time</a></th><th class='t-left'> <a onclick='sortTable(1)'>Raw Data</a> </th><th class='t-left'> <a onclick='sortTable(2)'>MQTT Topic</a> </th><th class='t-left'> <a onclick='sortTable(3)'>MQTT JSON</a> </th></tr>\r\n";
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
    htmlMessage += "var x = setInterval(function() {loadData(\"/data.txt\",updateData)}, 250);\r\n";       // update every 250 ms
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
    htmlMessage += " x = setInterval(function() {loadData(\"/data.txt\",updateData)}, 250);\r\n";       // update every 250 ms
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

	// Page end
	htmlMessage += FPSTR(htmlEnd);
	
    httpserver.send(200, "text/html", htmlMessage);
	
  }); // livedata
  
  httpserver.on("/reboot",[](){                                	// Reboot ESP
	DEBUG_PRINTLN("Html page requested: /reboot");
	
    DEBUG_PRINTLN("Rebooting ESP...");
    httpserver.send(200, "text/html", "<table class='condensed'><tr><td>Rebooting ESP...done</td></tr></table>");
	  delay(500);
    ESP.restart();
    //ESP.reset();
  });

  httpserver.on("/reset-mega",[](){                             // Reset MEGA
	DEBUG_PRINTLN("Html page requested: /reset-mega");
	
    DEBUG_PRINTLN("Resetting Mega...");
    httpserver.send(200, "text/html", "<table class='condensed'><tr><td>Resetting Mega... done</td></tr></table>");
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
    httpserver.sendHeader("Location","/");
    httpserver.send(303);
    
  });
  
  httpserver.on("/enable-debug",[](){
    DEBUG_PRINTLN("Enabling MQTT debug...");
    MQTT_DEBUG = 1;
    httpserver.sendHeader("Location","/infos");
    httpserver.send(303);
  });

  httpserver.on("/disable-debug",[](){
    DEBUG_PRINTLN("Disabling MQTT debug...");
    MQTT_DEBUG = 0;
	MQTTClient.publish(MQTT_DEBUG_TOPIC,"{\"DATA\":\" \",\"ID\":\" \",\"NAME\":\" \",\"TOPIC\":\" \",\"JSON\":\" \"}",MQTT_RETAIN_FLAG);
    httpserver.sendHeader("Location","/infos");
	httpserver.send(303);
  });
  
	//#ifdef ENABLE_ID_FILTERING_OPTION
	httpserver.on("/enable-id_filtering",[](){
	DEBUG_PRINTLN("Enabling ID filtering...");
	eepromConfig.id_filtering = 1;
	saveEEPROM();
	httpserver.sendHeader("Location","/infos");
	httpserver.send(303);
	});
	httpserver.on("/disable-id_filtering",[](){
		DEBUG_PRINTLN("Disabling ID filtering...");
		eepromConfig.id_filtering = 0;
		saveEEPROM();
		httpserver.sendHeader("Location","/infos");
		httpserver.send(303);
		});
	//#endif  
  
  httpserver.on("/data.txt", [](){			// Used to deliver raw data received (BUFFER) and mqtt data published (MQTT_TOPIC and JSON)           
    httpserver.send(200, "text/html", "<td>" + String(BUFFER) + "</td><td>" + String(MQTT_TOPIC) + "</td><td>" + String(JSON) + "</td>\r\n");
  });

  httpserver.on("/wifi-scan", [](){
	DEBUG_PRINTLN("Html page requested: /wifi-scan");
	int numberOfNetworks = WiFi.scanNetworks();
	String htmlMessage = "";
	htmlMessage += "<table class='condensed'>\r\n";
	for(int i =0; i<numberOfNetworks; i++){
	htmlMessage += "<tr><td>" + String(i+1) + " - " + String(WiFi.SSID(i)) + "</td><td>" + String(WiFi.RSSI(i)) + " dBm</td><tr>\r\n";
	}		
	htmlMessage += "</table>\r\n";
	httpserver.send(200, "text/html", htmlMessage);
  });

  httpserver.on("/infos", [](){
	DEBUG_PRINTLN("Html page requested: /infos");
	
	/*String htmlMessage = "<!DOCTYPE html>\r\n<html>\r\n";

    // Head
    htmlMessage += "<head><title>" + String(CLIENT_NAME) + " - Infos</title>\r\n";
    htmlMessage += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>\r\n";
    htmlMessage += "<meta http-equiv='Content-Type' content='text/html; charset=UTF-8' />\r\n";
	htmlMessage += "<link rel=\"stylesheet\" type=\"text/css\" href=\"/esp.css\">\r\n";
	//htmlMessage += "<style type='text/css'>" + String(FPSTR(cssDatasheet)) + "</style>\r\n";
    htmlMessage += "</head>\r\n";

    // Body
	htmlMessage += "<body class='bodymenu'>\r\n";
   
	
	// Header + menu
	htmlMessage += "<header class='headermenu'>";
	htmlMessage += "<h1>" + String(CLIENT_NAME) + "</h1>\r\n";
	htmlMessage += "<div class='menubar'><a class='menu' href='.'>&#8962;<span class='showmenulabel'>Home</span></a>\r\n";
	htmlMessage += "<a class='menu' href='/livedata'>&#10740;<span class='showmenulabel'>Live Data</span></a>\r\n";
	htmlMessage += "<a id='resetmega' class='menu' href='/reset-mega' onclick = \"fetch('/reset-mega');return false;\">&#128204;<span class='showmenulabel'>Reset MEGA</span></a>\r\n";
	htmlMessage += "<a class='menu' id='reboot' href='/reboot'>&#128268;<span class='showmenulabel'>Reboot ESP</span></a>\r\n";
	//htmlMessage += (!MQTT_DEBUG)? "<a class='menu' href='/enable-debug'>&#128172;<span class='showmenulabel'>Enable MQTT debug</span></a>\r\n":"<a class='menu' href='/disable-debug'>&#128172;<span class='showmenulabel'>Disable MQTT debug</span></a>\r\n";
	htmlMessage += "<a class='menu active' href='/infos'>&#128295;<span class='showmenulabel'>System Info</span></a></div>\r\n";
	htmlMessage += "</header>\r\n";	*/
	
	String htmlMessage = "";
	// Page start
	htmlMessage += FPSTR(htmlStart);
	
	// Header + menu
	htmlMessage += FPSTR(htmlMenu);
	htmlMessage += "<script>document.getElementById('menuinfos').classList.add('active');</script>";
   
    // System Info
    htmlMessage += "<h3>System Info</h3>\r\n";
    htmlMessage += "<table class='normal'>\r\n";
    htmlMessage += "<tr><td>Uptime</td><td>" + String(time_string_exp(millis())) + "</td></tr>\r\n";
	/*htmlMessage += "<script>"
		"function httpGetwifiscan() {"
		"const Http = new XMLHttpRequest();"
		"const url='/wifi-scan';"
		"Http.open(\"GET\", url);"
		"Http.send();"
		"Http.onreadystatechange = (e) => {"
		"	document.getElementById(\"wifiscan\").innerHTML = Http.responseText;"
		"}"
		"}"
		"</script>\r\n";*/
    htmlMessage += "<tr><td>WiFi network</td><td>" + String(WiFi.SSID()) + " " + WiFi.RSSI() + " dBm <a href='/wifi-scan' class='button link' onclick=\"fetchAndNotify('/wifi-scan');return false;\">Scan</a><div style='margin-top:0.25em;'id='wifiscan'></div>";
	#ifdef USE_AUTOCONNECT
		htmlMessage += "<a href='/_ac' class='button link'>AutoConnect Wifi settings</a> \r\n";
		htmlMessage += "<a href='/erase-wifi' class='button link'>Erase ALL Wifi settings</a> \r\n";
	#endif
	htmlMessage += "\r\n</td></tr>\r\n";
    htmlMessage += "<tr><td>IP address (MAC)</td><td>" + WiFi.localIP().toString() +" (" + String(WiFi.macAddress()) +")</td></tr>\r\n";
	htmlMessage += "<tr><td>ESP pin to reset MEGA</td><td>GPIO " + String(MEGA_RESET_PIN);
	if (resetMegaInterval > 0) {
		htmlMessage += " - auto reset after " + String( int(float(resetMegaInterval) *0.001 /60)) + " min if no data received - last data " + time_string_exp(now - lastReceived) + " ago";// - " + resetMegaCounter + " resets since ESP reboot";
	}
	htmlMessage += "</td></tr>";
    #ifdef ENABLE_MQTT_SETTINGS_ONLINE_CHANGE
		htmlMessage += "<tr><td>MQTT configuration</td><td><form method='post' action='/update-settings'><table class='condensed'><input type='hidden' name='save' value='1'>\r\n";
		htmlMessage += "<tr><td>server*</td><td><input type='text' name='mqtt_server' maxlength='" + String(CFG_MQTT_SERVER_SIZE) + "' value='" + String(eepromConfig.mqtt.server) + "'></td></tr>\r\n";
		htmlMessage += "<tr><td>port*</td><td><input type='number' style='width: 8em' name='mqtt_port' value='" + String(eepromConfig.mqtt.port) + "'></td></tr>\r\n";
		htmlMessage += "<tr><td>user</td><td><input type='text' name='mqtt_user' maxlength='" + String(CFG_MQTT_USER_SIZE) + "' value='" + String(eepromConfig.mqtt.user) + "'></td></tr>\r\n";
		htmlMessage += "<tr><td>password</td><td><input type='password' name='mqtt_password' maxlength='" + String(CFG_MQTT_PASSWORD_SIZE) + "' value='" + String(eepromConfig.mqtt.password) + "'></td></tr>\r\n";
		htmlMessage += "<tr><td></td><td><input type='submit' value='Apply'>*reboot required</td></tr></table></form></td></tr>\r\n";
	#else
		htmlMessage += "<tr><td>MQTT server and port</td><td>" + String(eepromConfig.mqtt.server) + ":" + String(eepromConfig.mqtt.port)+"</td></tr>\r\n";
	#endif
    htmlMessage += "<tr><td>MQTT connection state</td><td>" + String(MQTTClient.state()) + " <a class='button help' href='https://pubsubclient.knolleary.net/api.html#state' target='_blank'>&#10068;</a></td></tr>\r\n";
    htmlMessage += "<tr><td>MQTT connection attempts</td><td>" + String (mqttConnectionAttempts) + " since last reboot</td></tr>\r\n";
    htmlMessage += "<tr><td>MQTT debug</td><td>";
    (MQTT_DEBUG)? htmlMessage += "<span style=\"font-weight:bold\">enabled</span> | <a href='/disable-debug' class='button link'>Disable</a>" : htmlMessage += "disabled | <a href='/enable-debug' class='button link'>Enable</a>";
    htmlMessage += "</td></tr>\r\n";
	htmlMessage += "<tr><td>MQTT retain</td><td>" + String((MQTT_RETAIN_FLAG)? "true" : "false") + "</td></tr>\r\n";
	htmlMessage += "<tr><td>MQTT topics</td><td><table class='condensed'>\r\n";
    htmlMessage += "<tr><td>publish (json)</td><td> " + String(MQTT_PUBLISH_TOPIC) + "/Protocol_Name-ID</td></tr>\r\n";
    htmlMessage += "<tr><td>commands to rflink</td><td> " + String(MQTT_RFLINK_CMD_TOPIC) + "</td></tr>\r\n";
    htmlMessage += "<tr><td>last will ( " + String(MQTT_WILL_ONLINE) + " / " + String(MQTT_WILL_OFFLINE) + " )</td><td> " + String(MQTT_WILL_TOPIC) + "</td></tr>\r\n";
    htmlMessage += "<tr><td>uptime (min, every " + String( int( float(uptimeInterval) *0.001 / 60) ) + ")</td><td> " + String(MQTT_UPTIME_TOPIC) + "</td></tr>\r\n";
    htmlMessage += "<tr><td>debug (data from rflink)</td><td> " + String(MQTT_DEBUG_TOPIC) + "</td></tr>\r\n";
  if (resetMegaInterval > 0) {
    htmlMessage += "<tr><td>mega reset information</td><td> " + String(MQTT_MEGA_RESET_TOPIC) + "</td></tr>\r\n";
  }
    htmlMessage += "</table></td></tr>\r\n";
    for (int i = 0; i < (int) (sizeof(user_specific_ids) / sizeof(user_specific_ids[0])); i++){      // User specific IDs defined in user_specific_ids
          htmlMessage += "<tr><td>User specific</td><td>ID for protocol " + String(user_specific_ids[i][0]) + " is forced to " + String(user_specific_ids[i][2]) + "; applies to ID: " + String(user_specific_ids[i][1]) +"</td></tr>\r\n";
    }
    //register uint32_t *sp asm("a1");
    //htmlMessage += "<tr><td>Free Stack</td><td>" +  String(4 * (sp - g_pcont->stack) ) + "</td></tr>\r\n";
	//#ifdef ENABLE_ID_FILTERING_OPTION
	htmlMessage += "<tr><td>ID filtering</td><td>";
	(eepromConfig.id_filtering)? htmlMessage += "<span style=\"font-weight:bold\">enabled</span> | <a class='button link' href='/configuration'>Configure</a> | <a href='/disable-id_filtering' class='button link'>Disable</a> " : htmlMessage += "disabled | <a href='/enable-id_filtering' class='button link'>Enable</a>";
	htmlMessage += "</td></tr>\r\n";
	//#endif
	htmlMessage += "<tr><td style='min-width:150px;'>Config version</td><td style='width:80%;'>" + String(CONFIG_VERSION) + "</td></tr>\r\n";
	htmlMessage += "</table>\r\n";
	
	// Filesystem
    htmlMessage += "<h3>Firmware and EEPROM</h3>\r\n";
    htmlMessage += "<table class='normal'>\r\n";
	htmlMessage += "<td style='min-width:150px;'><a href='/update' class='button link'>Load&nbsp;firmware</a></td><td style='width:80%;'>Load a new firmware for the ESP</td>\r\n";
	#ifdef ENABLE_SERIAL_DEBUG
		htmlMessage += "<tr><td><a href='/read-eeprom' class='button link'>Read&nbsp;EEPROM</a></td><td>Outputs EEPROM content to serial debug</td></tr>\r\n";
	#endif
	htmlMessage += "<tr><td><a href='/erase-eeprom' class='button link'>Erase&nbsp;EEPROM</a></td><td>Deletes Wifi settings, MQTT settings, IP filtering configuration and reloads default values from firmware (config.h)</td><tr>\r\n";
    #ifdef EXPERIMENTAL
		htmlMessage += "<tr><td>RFLink packet lost</td><td>" + String (lost_packets) + "</td></tr>\r\n"; // TEST packet lost
	#endif
	htmlMessage += "<tr><td> Compile date</td><td>" + String (__DATE__ " " __TIME__) + "</td></tr>\r\n";
	htmlMessage += "<tr><td> FreeMem</td><td>" + String (ESP.getFreeHeap()) + "</td></tr>\r\n";
    htmlMessage += "<tr><td> Heap Max Free Block</td><td>" + String(ESP.getMaxFreeBlockSize()) + "</td></tr>\r\n"; 
	htmlMessage += "<tr><td> Heap Fragmentation</td><td>" + String (ESP.getHeapFragmentation()) + "%</td></tr>\r\n";
	htmlMessage += "</table>\r\n";

	// Page end
	htmlMessage += FPSTR(htmlEnd);
	
    httpserver.send(200, "text/html", htmlMessage);
  });

  httpserver.on("/update-settings", [](){                // Used to change settings in EEPROM
	DEBUG_PRINTLN("Html page requested: /update-settings");
	// URL arguments
	String htmlMessage = "Settings updated";
	//showEEPROM();
	if (httpserver.hasArg("save")) {
		int itemp;
		strncpy(eepromConfig.mqtt.server 	,   httpserver.arg("mqtt_server").c_str(),     	CFG_MQTT_SERVER_SIZE);
		itemp = httpserver.arg("mqtt_port").toInt();
		eepromConfig.mqtt.port = (itemp>0 && itemp<=65535) ? itemp : MQTT_PORT;
		strncpy(eepromConfig.mqtt.user 		,   httpserver.arg("mqtt_user").c_str(),     	CFG_MQTT_USER_SIZE);
		strncpy(eepromConfig.mqtt.password	,   httpserver.arg("mqtt_password").c_str(),    CFG_MQTT_PASSWORD_SIZE);
	}
	saveEEPROM();
	DEBUG_PRINTLN("Settings updated. New EEPROM configuration is:");
	loadEEPROM();
	showEEPROM();
	//httpserver.send(200, "text/html", htmlMessage);
	httpserver.sendHeader("Location","/infos");
	httpserver.send(303);
  });
	
  httpserver.on("/configuration", [](){                // Used to change filtered_IDs configuration in EEPROM
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
	  if (i <= filtered_id_number) {
		arg_id.toCharArray(eepromConfig.filtered_id[i].id,MAX_ID_LEN+1);
		arg_id_applied.toCharArray(eepromConfig.filtered_id[i].id_applied,MAX_ID_LEN+1);
		arg_description.toCharArray(eepromConfig.filtered_id[i].description,MAX_DATA_LEN+1);
		eepromConfig.filtered_id[i].publish_interval = arg_publish_interval.toInt();
	  }
	}
	
	/*
    String htmlMessage = "<!DOCTYPE html>\r\n<html>\r\n";

    // Head
    htmlMessage += "<head>\r\n<title>" + String(CLIENT_NAME) + " - Configuration</title>\r\n";
    htmlMessage += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>\r\n";
    htmlMessage += "<meta http-equiv='Content-Type' content='text/html; charset=UTF-8' />\r\n";
	htmlMessage += "<link rel=\"stylesheet\" type=\"text/css\" href=\"/esp.css\">\r\n";
	//htmlMessage += "<style type='text/css'>" + String(FPSTR(cssDatasheet)) + "</style>\r\n";
    htmlMessage += "</head>\r\n";
	*/
	
	String htmlMessage = "";
	// Page start
	htmlMessage += FPSTR(htmlStart);
	
	// Header + menu
	htmlMessage += FPSTR(htmlMenu);
	//htmlMessage += "<script>document.getElementById('menulivedata').classList.add('active');</script>";

    // Body
	//htmlMessage += "<body class='bodymenu'>\r\n";
	//htmlMessage += "<a href='/infos' class='button link'>System Info</a>\r\n";
	//htmlMessage += "<br><br>\r\n";
   
	if (eepromConfig.id_filtering) {                      // show list of user IDs
		htmlMessage += "<h3>Filtered IDs current configuration</h3><br>\r\n";
		if (filtered_id_number > 10) {
			for (int i = 0; i <= ((int) (filtered_id_number-1)/10); i++){
			   htmlMessage += "<a href='/configuration?page=" + String(i) + "' class='button link'>" + String(i*10+1) + " - " + String(min(filtered_id_number,i*10+10)) + "</a>\r\n";
			}
			htmlMessage += "<br><br>\r\n";
		}
		htmlMessage += "<div class='note'>Changes can be done one line at a time by clicking on \"Apply\" at the right.<br>Then, configuration should be saved by clicking on \"Save to EEPROM\"</div><br>\r\n";

		htmlMessage += "<table class='multirow'><tr><th>#</th><th>ID</th><th>ID applied</th><th>Description</th><th>Interval (ms)  </th><th></th></tr>\r\n";
		for (int i = 0+(10*page); i < min(filtered_id_number,10*(page+1)); i++){
			htmlMessage += "<tr><form method='post' action='/configuration'>";
			htmlMessage += "<td><input type='hidden' name='line' value=" + String(i) + "><input type='hidden' name='page' value=" + page + ">" + String(i+1) + "</td>";
			htmlMessage += "<td><input type='text' name='id' maxlength='" + String(MAX_ID_LEN) + "' size='" + String(MAX_ID_LEN) + "' value='" + String(eepromConfig.filtered_id[i].id) + "'></td>";
			htmlMessage += "<td><input type='text' name='id_applied' maxlength='" + String(MAX_ID_LEN) + "' size='" + String(MAX_ID_LEN) + "' value='" + String(eepromConfig.filtered_id[i].id_applied) + "'></td>";
			htmlMessage += "<td class='t-left'><input type='text' name='description' maxlength='" + String(MAX_DATA_LEN) + "' size='" + String(MAX_DATA_LEN) + "' value='" + String(eepromConfig.filtered_id[i].description) + "'></td>";
			htmlMessage += "<td><input type='number' style='width: 8em' name='publish_interval' step='1000' value='" + String(eepromConfig.filtered_id[i].publish_interval) + "'></td>";
			htmlMessage += "<td><input type='submit' value='Apply'></td></form></tr>\r\n";
		}
		htmlMessage += "</table><br>\r\n";
		htmlMessage += "<a class='button link' href='/save-eeprom'>Save to EEPROM</a> => Save above configuration to EEPROM (otherwise changes are lost on reboot)";  
		htmlMessage += "<br><br><br><div style='font-size:0.8em;'>In case you compile your own firmware, you can update easily config.h file with current configuration with <a style='font-size:0.8em;' href='javascript:void(0)' onclick=\"document.getElementById('code_config_h').style.display = 'block';return false;\">this code</a><br>\r\n";
		htmlMessage += "<xmp id='code_config_h' style='display:none;'>\r\n";
		  
		for (int i = 0; i < (filtered_id_number); i++){
			String str_description = String(eepromConfig.filtered_id[i].description);     
			htmlMessage += "  {\"" + String(eepromConfig.filtered_id[i].id) + "\",\"" + String(eepromConfig.filtered_id[i].id_applied) + "\"," + String(eepromConfig.filtered_id[i].publish_interval) + ",\"" + str_description + "\"}, //" + (i+1) + "\r\n";
		}
		htmlMessage += "</xmp></div>\r\n";
    } else {
		htmlMessage += "<h3>ID filtering is disabled</h3><br>\r\n";
	}
	
	// Page end
	htmlMessage += FPSTR(htmlEnd);
	
	DEBUG_PRINT("Free mem: ");DEBUG_PRINTLN(ESP.getFreeHeap());
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
    DEBUG_PRINTLN("Erasing EEPROM ID filtering configuration");
	DEBUG_PRINT("eepromLength: ");DEBUG_PRINTLN(eepromLength);
  	EEPROM.begin(4096);
  	for (int i = eepromAddress; i < (eepromLength); i++) {
  	EEPROM.write(i, 0);
  	}
    EEPROM.end();
    DEBUG_PRINTLN("Rebooting device...");
    httpserver.send(200, "text/html", "EEPROM configuration erased, rebooting... <a href='/infos'>System</a>");
    delay(500);
    ESP.restart();
    //ESP.reset();
  });
  
	#ifdef ENABLE_SERIAL_DEBUG
		httpserver.on("/read-eeprom",[](){                                  // Erasing EEPROM + reboot
		DEBUG_PRINTLN("Html page requested: /read-eeprom");
		DEBUG_PRINTLN("Reading EEPROM to serial debug");
		#define sizeBytes 4096
		#define listBytes 4096
		int startByte = 0;
		int endByte = 0;
		  EEPROM.begin(sizeBytes);
		  delay(100);

		  DEBUG_PRINTLN("**********************************************************************************************");
		  DEBUG_PRINTLN("");
		  DEBUG_PRINT("             EEPROM-size defined with ");
		  DEBUG_PRINT(sizeBytes);
		  DEBUG_PRINT(" bytes and list from byte: ");
		  DEBUG_PRINT(startByte);
		  DEBUG_PRINT(" - ");
		  DEBUG_PRINTLN(listBytes - 1);
		  DEBUG_PRINTLN("");
		  String content;String con;
		  DEBUG_PRINTLN("---------------------------------------------------------------------------------------------|");
		  DEBUG_PRINTLN("                   |  0000000000111111  |                                                    |");
		  DEBUG_PRINTLN("                   |  0123456789012345  |  00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15   |");
		  DEBUG_PRINTLN("---------------------------------------------------------------------------------------------|");
		for (int j = 0 ; j < (sizeBytes / 1024) ; j++) {
			startByte = j*1024;
			endByte = (j+1)*1024;
		  for (int i = startByte; i < endByte; i = i + 16) {
			for (int x = i; x < i + 16; ++x) {			
			  con = char(EEPROM.read(x));
			  //                                 Char
			  //----------------------------------------
			  if (EEPROM.read(x) ==   0 ||   //  (nul)
				  EEPROM.read(x) ==   1 ||   //  (soh)
				  EEPROM.read(x) ==   2 ||   //  (stx)
				  EEPROM.read(x) ==   3 ||   //  (ext)
				  EEPROM.read(x) ==   4 ||   //  (eot)
				  EEPROM.read(x) ==   5 ||   //  (enq)
				  EEPROM.read(x) ==   6 ||   //  (ack)
				  EEPROM.read(x) ==   7 ||   //  (bel)
				  EEPROM.read(x) ==   8 ||   //  (bs)
				  EEPROM.read(x) ==   9 ||   //  (ht)
				  EEPROM.read(x) ==  10 ||   //  (nl)
				  EEPROM.read(x) ==  11 ||   //  (vt)
				  EEPROM.read(x) ==  12 ||   //  (np)
				  EEPROM.read(x) ==  13 ||   //  (cr)
				  EEPROM.read(x) ==  14 ||   //  (so)
				  EEPROM.read(x) ==  15 ||   //  (si)
				  EEPROM.read(x) ==  16 ||   //  (dle)
				  EEPROM.read(x) ==  17 ||   //  (dc1)
				  EEPROM.read(x) ==  18 ||   //  (dc2)
				  EEPROM.read(x) ==  19 ||   //  (dc3)
				  EEPROM.read(x) ==  20 ||   //  (dc4)
				  EEPROM.read(x) ==  21 ||   //  (nak)
				  EEPROM.read(x) ==  22 ||   //  (syn)
				  EEPROM.read(x) ==  23 ||   //  (etb)
				  EEPROM.read(x) ==  24 ||   //  (can)
				  EEPROM.read(x) ==  25 ||   //  (em)
				  EEPROM.read(x) ==  26 ||   //  (sub)
				  EEPROM.read(x) ==  27 ||   //  (esc)
				  EEPROM.read(x) ==  28 ||   //  (fs)
				  EEPROM.read(x) ==  29 ||   //  (gs)
				  EEPROM.read(x) ==  30 ||   //  (rs)
				  EEPROM.read(x) ==  31 ||   //  (us)
				  EEPROM.read(x) == 127 ) {  //  (del)
				con = " ";
			  }
			  content += con;
			}
			DEBUG_PRINTF("Byte: %4d", i);
			DEBUG_PRINT(" - ");
			DEBUG_PRINTF("%4d", i + 15);
			DEBUG_PRINT("  |  ");
			DEBUG_PRINT(content);
			DEBUG_PRINT("  |  ");
			content = "";
			for (int y = i; y < i + 16; ++y) {
			  DEBUG_PRINTF("%02x ", EEPROM.read(y));
			}
			DEBUG_PRINTLN("  |");
			delay(2);
		  }
		}
		  DEBUG_PRINTLN("---------------------------------------------------------------------------------------------|");
		  DEBUG_PRINTLN("");
		  delay(100);
		EEPROM.end();
		//httpserver.send(200, "text/html", "EEPROM read to serial <a href='/infos'>System Info</a>");
		httpserver.sendHeader("Location","/infos");
		httpserver.send(303);
  
		});
	#endif

	#ifdef USE_AUTOCONNECT
	  httpserver.on("/erase-wifi",[](){                                  // Erasing EEPROM + reboot
		DEBUG_PRINTLN("Html page requested: /erase-wifi");
		DEBUG_PRINTLN("Erasing AutoConnect Wifi settings");
		deleteAllCredentials();
		delay(200);
		DEBUG_PRINTLN("Rebooting device...");
		httpserver.send(200, "text/html", "AutoConnect Wifi settings erased, rebooting... <a href='/infos'>System Info</a>");
		delay(200);
		WiFi.disconnect();
		delay(500);
		//ESP.restart();
		ESP.reset();
	  });
	#endif
 
 
  httpserver.onNotFound([](){
    httpserver.send(404, "text/plain", "404: Not found");      	// Send HTTP status 404 (Not Found) when there's no handler for the URI in the request
  });

};

/**
 * Time functions
 */

long uptime_min()
{
 //long days=0;
 //long hours=0;
 long mins=0;
 long secs=0;
 secs = millis()/1000; 		//convect milliseconds to seconds
 mins=secs/60; 				//convert seconds to minutes
 //hours=mins/60; 			//convert minutes to hours
 //days=hours/24; 			//convert hours to days
 return mins;
}

String time_string_exp(long time)
{
  String result;
  char strUpTime[40];
  int minutes = int(float(time) *0.001 / 60);
  int days = minutes / 1440;
  minutes = minutes % 1440;
  int hrs = minutes / 60;
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

/**
 * Fonction de calcul d'une somme de contrôle CRC-16 CCITT 0xFFFF
 */

uint16_t crc16_ccitt(unsigned char* data, unsigned int data_len) {
  uint16_t crc = 0xFFFF;

  if (data_len == 0)
    return 0;

  for (unsigned int i = 0; i < data_len; ++i) {
    uint16_t dbyte = data[i];
    crc ^= dbyte << 8;

    for (unsigned char j = 0; j < 8; ++j) {
      uint16_t mix = crc & 0x8000;
      crc = (crc << 1);
      if (mix)
        crc = crc ^ 0x1021;
    }
  }

  return crc;
}

//********************************************************************************
// Setup
//********************************************************************************

void setup() {
	
	#ifdef ENABLE_SERIAL_DEBUG
		debugSerialTX.begin(115200);        // debug serial : same speed as RFLInk to be compatible if same port used
			while (!debugSerialTX) {
					; // wait for serial port to connect. Needed for native USB port only
			}
		delay(1000);
		DEBUG_PRINTLN();
		DEBUG_PRINTLN(F("Init debug serial done"));
	#endif

	// Set the data rate for the RFLink serial
	rflinkSerialTX.begin(57600);
	delay(1000);
	DEBUG_PRINTLN()
	DEBUG_PRINTLN(F("Starting..."));
	DEBUG_PRINTLN(F("Init rflink serial done"));

    loadEEPROM();
		
	checkEEPROM();
	
	MQTTClient.setClient(wifiClient);
	MQTTClient.setServer(eepromConfig.mqtt.server,eepromConfig.mqtt.port);
	MQTTClient.setCallback (callback);
	
	// Setup OTA
	SetupOTA();
	ArduinoOTA.begin();
	DEBUG_PRINTLN("Ready for OTA");
		
	// Setup HTTP Server
	ConfigHTTPserver(); 
	// Setup http update server for firmware upgrade from /update
	httpUpdater.setup(&httpserver);
	DEBUG_PRINTLN("HTTPUpdateServer ready!");
	
	#ifdef USE_AUTOCONNECT
		mqtt.add({header, caption});
		portal.join({ mqtt, update });
		setup_wifi_autoconnect();
		DEBUG_PRINTLN("HTTP server started by Autoconnect");
	#elif USE_WM
		setup_wifi_wm();
		httpserver.begin();
	#else
		setup_wifi(); 
		// Start the HTTP server
		httpserver.begin();
		DEBUG_PRINTLN("HTTP server started");
	#endif
	
	DEBUG_PRINT("Size of eepromConfig: ");DEBUG_PRINTLN(sizeof(eepromConfig));
	DEBUG_PRINT("Size of filtered_IDs: ");DEBUG_PRINTLN(sizeof(filtered_IDs));
	DEBUG_PRINT("Size of matrix: ");DEBUG_PRINTLN(sizeof(matrix));
		
	//rflinkSerialTX.println();
	delay(1000);
	//rflinkSerialTX.println(F("10;status;"));                  // Ask status to RFLink
	rflinkSerialTX.println();
	rflinkSerialTX.println(F("10;ping;"));                        // Do a PING on startup
	delay(500);
	rflinkSerialTX.println(F("10;version;"));                     // Ask version to RFLink
        
} // setup

//********************************************************************************
// Main loop
//********************************************************************************

void loop() {
	bool DataReady=false;
	// handle lost of connection : retry after 1s on each loop
	if (!MQTTClient.connected()) {
		//delay(1000);
		now = millis();
		if (now - lastMqttConnect >= 1000) {
			DEBUG_PRINTLN(F("MQTT not connected, retrying after 1s"));
			lastMqttConnect = now;
			MqttConnect();
		}
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
			
			#ifdef EXPERIMENTAL
				// check if a line was lost TEST
				//DEBUG_PRINTLN(LINE_NUMBER);
				byte expected_line_number = line_number + 1;//DEBUG_PRINTLN(expected_line_number);                        
				line_number = strtoul(LINE_NUMBER,NULL,16);//DEBUG_PRINTLN(line_number);
				if (lost_packets == -1) {
				  lost_packets = 0;//DEBUG_PRINTLN(lost_packets);
				} else {
				  lost_packets += line_number - expected_line_number;//DEBUG_PRINTLN(lost_packets);
				  //MQTTClient.publish("rflink/lost_packets",(char*) lost_packets);
				}
			#endif

			// Store last received data time if MQTT_ID is valid
			if ( (strcmp(MQTT_ID,"") != 0) && (strcmp(MQTT_ID,"0\0") != 0) ) lastReceived = millis();
			
			// If user_specific_ids is used
			for (int i = 0; i < (int) (sizeof(user_specific_ids) / sizeof(user_specific_ids[0])); i++){
			  if (strcmp(MQTT_NAME,user_specific_ids[i][0]) == 0){    			// check if protocol / name matches
				if (strcmp(user_specific_ids[i][1],"ALL") == 0) {   		// applies new ID to all IDs for specific name/protocol
					strcpy( MQTT_ID , user_specific_ids[i][2]);
				} else if (strcmp(user_specific_ids[i][1],MQTT_ID) == 0) {
					strcpy( MQTT_ID , user_specific_ids[i][2]);       		// applies new ID to specific ID for specific name/protocol
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
				for (char* p = JSON_DEBUG; (p = strchr(p, '\"')) ; ++p) {*p = '\'';} // Remove quotes
				strcat(BUFFER_DEBUG,"\"JSON\":\"");strcat(BUFFER_DEBUG,JSON_DEBUG);strcat(BUFFER_DEBUG,"\"");
				strcat(BUFFER_DEBUG,",\"ID\":\"");strcat(BUFFER_DEBUG,MQTT_ID);strcat(BUFFER_DEBUG,"\"");
				strcat(BUFFER_DEBUG,"}");
				MQTTClient.publish(MQTT_DEBUG_TOPIC,BUFFER_DEBUG);
				}
			
			// MQTT debug mode, raw data only	
			//if (MQTT_DEBUG) MQTTClient.publish(MQTT_DEBUG_TOPIC,BUFFER);      

			
			// XXX publish to MQTT server only if ID is authorized
			
			// ID filtering with filtered_IDs 
			if (eepromConfig.id_filtering) {                         
				for (int i = 0; i < (filtered_id_number); i++){
					
					DEBUG_PRINT("ID = ");DEBUG_PRINT(eepromConfig.filtered_id[i].id);
					DEBUG_PRINT(" ; ID_applied = ");DEBUG_PRINT(eepromConfig.filtered_id[i].id_applied);
					DEBUG_PRINT(" ; publish_interval = ");DEBUG_PRINT(eepromConfig.filtered_id[i].publish_interval);
					DEBUG_PRINT(" ; decription = ");DEBUG_PRINT(eepromConfig.filtered_id[i].description);
					DEBUG_PRINT(" ; json = ");DEBUG_PRINT(matrix[i].json);
					//DEBUG_PRINT(" ; json_checksum = ");DEBUG_PRINT(matrix[i].json_checksum);
					DEBUG_PRINT(" ; last_published = ");DEBUG_PRINT(matrix[i].last_published);
					DEBUG_PRINT(" ; last_received = ");DEBUG_PRINTLN(matrix[i].last_received);

					if (strcmp(MQTT_ID,eepromConfig.filtered_id[i].id) == 0) {                          // check ID is authorized
					  DEBUG_PRINT("Authorized ID ");DEBUG_PRINT(MQTT_ID);
					  matrix[i].last_received = millis();                                         	// memorize received time
					  if (strcmp(eepromConfig.filtered_id[i].id_applied,"") != 0 ) {					// apply different ID if available
						 strcpy(MQTT_ID,eepromConfig.filtered_id[i].id_applied);
						 buildMqttTopic();															// rebuild MQTT_TOPIC
						}
					  //uint16_t json_checksum = crc16_ccitt((byte*) &JSON, sizeof(JSON));
					  if (strncmp(matrix[i].json,JSON,sizeof(matrix[i].json)-2) != 0) {                                     	// check if json data has changed
					  //if (matrix[i].json_checksum != json_checksum) {                                   // check if json_checksum has changed
						DEBUG_PRINT(" => data changed => published on ");DEBUG_PRINTLN(MQTT_TOPIC);
						MQTTClient.publish(MQTT_TOPIC,JSON,MQTT_RETAIN_FLAG);                           			  	// if changed, publish on MQTT server  
						strncpy(matrix[i].json,JSON,sizeof(matrix[i].json)-2);                       	// memorize new json value
						//matrix[i].json_checksum = json_checksum;                                 	// memorize new json_checksum
						matrix[i].last_published = millis();                                      	// memorize published time           
					  } else {                                                                    	// no data change
						now = millis();
						if ( (now - matrix[i].last_published) > (eepromConfig.filtered_id[i].publish_interval) ) {   	// check if it exceeded time for last publish
						  DEBUG_PRINT(" => no data change but max time interval exceeded => published on ");DEBUG_PRINTLN(MQTT_TOPIC);
						  MQTTClient.publish(MQTT_TOPIC,JSON,MQTT_RETAIN_FLAG);                             			// publish on MQTT server
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
				MQTTClient.publish(MQTT_TOPIC,JSON,MQTT_RETAIN_FLAG);                   	// publish on MQTT server
			}
			
		} // end of DataReady

		now = millis();
		
		// Handle uptime
		if ( (now - lastUptime) > (uptimeInterval) ) {    				// if uptime interval is exceeded
			char mqtt_publish_payload[50];
			lastUptime = now;
			sprintf(mqtt_publish_payload,"%ld", uptime_min());
			MQTTClient.publish(MQTT_UPTIME_TOPIC,mqtt_publish_payload,MQTT_RETAIN_FLAG);
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
	#ifdef USE_AUTOCONNECT
		portal.handleRequest(); // Handle Autoconnect menu
		if (WiFi.status() == WL_IDLE_STATUS) {
			ESP.reset();
			delay(1000);
		}
	#endif
	
} // loop
