
// User configuration is to be done in config.h file

// Global definition file
#include "espRFLinkMQTT.h"

//********************************************************************************
// Declare WiFi and MQTT
//********************************************************************************

WiFiClient wifiClient;

PubSubClient MQTTClient;

//PubSubClient MQTTClient(eepromConfig.mqtt.server, eepromConfig.mqtt.port, callback, wifiClient);

ESP8266WebServer httpserver(80);                               // Create a webserver object that listens for HTTP request on port 80

ESP8266HTTPUpdateServer httpUpdater;                           // Firmware webupdate

//********************************************************************************
// Functions
//********************************************************************************

/** Serial debug functions */
#if defined(ENABLE_SERIAL_DEBUG) 
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
	  DEBUG_PRINT("EEPROM configuration max size: ");DEBUG_PRINTLN(eepromConfigMaxSize);
	  EEPROM.get(eepromAddress,eepromConfig);  
	  EEPROM.end();
}

/** Show eepromConfig from EEPROM memory */
void showEEPROM() {
		DEBUG_PRINTLN("eepromConfig content:");
		DEBUG_PRINTLN(" - WiFi");
		DEBUG_PRINT(" \t| ssid: ");DEBUG_PRINTLN(eepromConfig.ssid);
		DEBUG_PRINT(" \t| password: ");DEBUG_PRINTLN(eepromConfig.psk);
		DEBUG_PRINT(" \t| hostname: ");DEBUG_PRINTLN(eepromConfig.hostname);
		DEBUG_PRINTLN(" - MQTT");
		DEBUG_PRINT(" \t| server: ");DEBUG_PRINTLN(eepromConfig.mqtt.server);
		DEBUG_PRINT(" \t| port: ");DEBUG_PRINTLN(eepromConfig.mqtt.port);
		DEBUG_PRINT(" \t| user: ");DEBUG_PRINTLN(eepromConfig.mqtt.user);
		DEBUG_PRINT(" \t| password: ");DEBUG_PRINTLN(eepromConfig.mqtt.password);
		DEBUG_PRINT(" - ID filtering: ");DEBUG_PRINTLN((eepromConfig.id_filtering)?"enabled":"disabled");
		for (int i =0; i < FILTERED_ID_SIZE; i++) {
			DEBUG_PRINT(" \t| ");DEBUG_PRINT(eepromConfig.filtered_id[i].id);
			DEBUG_PRINT(" \t\t- ");DEBUG_PRINT(eepromConfig.filtered_id[i].id_applied);  
			DEBUG_PRINT(" \t\t- ");DEBUG_PRINT(eepromConfig.filtered_id[i].publish_interval);
			DEBUG_PRINT(" \t\t- ");DEBUG_PRINT(eepromConfig.filtered_id[i].description);
			DEBUG_PRINTLN();
		}
		DEBUG_PRINT(" - Version ");DEBUG_PRINTLN(eepromConfig.version);
};

/** Check config from EEPROM memory */
void checkEEPROM() {
	
	// Check EEPROM version configuration
	DEBUG_PRINT("Config version in EEPROM:    ")DEBUG_PRINTLN(eepromConfig.version);
	DEBUG_PRINT("Config version in config.h:  ")DEBUG_PRINTLN(CONFIG_VERSION);
	
	if (eepromConfig.version != CONFIG_VERSION) {
		
		DEBUG_PRINTLN("Config version changed => initialize EEPROM configuration from firmware (config.h)");

		// Use WiFi settings from config.h
		strncpy(eepromConfig.ssid,WIFI_SSID,CFG_SSID_SIZE);
		strncpy(eepromConfig.psk,WIFI_PASSWORD,CFG_PSK_SIZE);
		strncpy(eepromConfig.hostname,HOSTNAME,CFG_HOSTNAME_SIZE);
		
		// Use MQTT settings from config.h
		strncpy(eepromConfig.mqtt.server,MQTT_SERVER,CFG_MQTT_SERVER_SIZE);
		eepromConfig.mqtt.port = MQTT_PORT;
		strncpy(eepromConfig.mqtt.user,MQTT_USER,CFG_MQTT_USER_SIZE);
		strncpy(eepromConfig.mqtt.password,MQTT_PASSWORD,CFG_MQTT_PASSWORD_SIZE);
		
		// Use ID filtering configuration from config.h
		eepromConfig.id_filtering = ID_FILTERING;
		for (int i =0; i < filtered_id_number; i++) {
			strcpy(eepromConfig.filtered_id[i].id,filtered_IDs[i].id);
			strcpy(eepromConfig.filtered_id[i].id_applied,filtered_IDs[i].id_applied);
			eepromConfig.filtered_id[i].publish_interval = filtered_IDs[i].publish_interval;
			strcpy(eepromConfig.filtered_id[i].description,filtered_IDs[i].description);
		}
		
		// Save EEPROM
		saveEEPROM();
		loadEEPROM();
	}
	else DEBUG_PRINTLN("Version did not change - using current EEPROM configuration");
		
	showEEPROM();
}

void setup_simple_wifi() {
	delay(10);
	DEBUG_PRINT("Starting WiFi, connecting to '");
	DEBUG_PRINT(eepromConfig.ssid); DEBUG_PRINTLN("' ...");
	WiFi.hostname(HOSTNAME);
	WiFi.mode(WIFI_STA); 											// Act as wifi_client only, defaults to act as both a wifi_client and an access-point.
	WiFi.begin(eepromConfig.ssid,eepromConfig.psk);					// Connect to the network
	int i = 0;
	while (WiFi.status() != WL_CONNECTED) {							// Wait for the Wi-Fi to connect
	  delay(1000); i++;
	  DEBUG_PRINT(i); DEBUG_PRINT(' ');
	};
	DEBUG_PRINTLN();
	DEBUG_PRINTLN("WiFi connected to " + WiFi.SSID() + ", IP address:\t" + WiFi.localIP().toString());
}

#ifdef ENABLE_WIFI_SETTINGS_ONLINE_CHANGE

	void setup_complex_wifi() {
		delay(10);
		DEBUG_PRINT("Starting complex WiFi, connecting to '");
		DEBUG_PRINT(eepromConfig.ssid); DEBUG_PRINTLN("' ...");
		WiFi.hostname(HOSTNAME);
		WiFi.mode(WIFI_STA); 										// Act as wifi_client only
		WiFi.begin(eepromConfig.ssid,eepromConfig.psk);				// Connect to the network
		int i = (int) (WIFI_CONNECT_TIMEOUT/1000);
		unsigned long startedAt = millis();
		// Wait for the Wi-Fi to connect or timeout is reached
		while ( (WiFi.status() != WL_CONNECTED) && ( (millis() - startedAt) < WIFI_CONNECT_TIMEOUT ) )
			{
			delay(1000);
			DEBUG_PRINT(i); DEBUG_PRINT(' ');i--;
			}
		DEBUG_PRINTLN();
		
		if (WiFi.status() == WL_CONNECTED) {						// WiFi connection succesfull
			DEBUG_PRINTLN("WiFi connected to " + WiFi.SSID() + ", IP address:\t" + WiFi.localIP().toString());
			
		} else {													// WiFi connection timeout reached, starting AP

			DEBUG_PRINTLN("WiFi connection failed within " + String((int)(WIFI_CONNECT_TIMEOUT/1000)) + " seconds: starting AP during " + String(WIFI_AP_TIMEOUT/60000) + " minutes." );
			WiFi.softAP(eepromConfig.hostname);
			WiFi.mode(WIFI_AP);
			DEBUG_PRINTLN("Access Point started with WiFi name " + String(eepromConfig.hostname) + ", IP address:\t" + WiFi.softAPIP().toString());
			
			// Start the HTTP server
			DEBUG_PRINTLN("HTTP server started for AP");
			httpserver.begin();

			// Keep AP till timeout is reached
			startedAt = millis();
			int i = (int) (WIFI_AP_TIMEOUT/1000);
			bool unique = 0;
			while ( ( (millis() - startedAt) < WIFI_AP_TIMEOUT ) ) {
				if (millis()%1000 == 0) { // every second
					if (unique == 0) {
						DEBUG_PRINT(i); DEBUG_PRINT(' ');i--;
						unique = 1;
					}
				} else {
					unique = 0;
				}
				yield();
				httpserver.handleClient();
			}
			DEBUG_PRINTLN();

			// AP timeout reached, going back to normal WiFi connection
			DEBUG_PRINTLN("AP timeout reached within " + String((int)(WIFI_AP_TIMEOUT/60000)) + " minutes.");
			DEBUG_PRINTLN("Going back to normal WiFi connection");
			setup_simple_wifi();
		}
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
 
boolean mqttConnect() {

	mqttConnectionAttempts++;
	DEBUG_PRINT(F("MQTT connection attempts: "));DEBUG_PRINTLN(mqttConnectionAttempts);

	// Generate unique client id
	char uniqueId[6];
	uniqueId[0] = '-';
	for(int i = 1; i<=4; i++){uniqueId[i]= 0x30 | random(0,10);}
	uniqueId[5] = '\0';
	char clientId[MAX_TOPIC_LEN] = "";
	strcpy(clientId, HOSTNAME);
	strcat(clientId, uniqueId);
	DEBUG_PRINT("MQTT clientId: ");DEBUG_PRINTLN(clientId);

	// Connect to MQTT broker 
	if (MQTTClient.connect(clientId, eepromConfig.mqtt.user, eepromConfig.mqtt.password, MQTT_WILL_TOPIC, 0, 1, MQTT_WILL_OFFLINE,1)) {
		MQTTClient.subscribe(MQTT_RFLINK_CMD_TOPIC);				// subcribe to cmd topic
		MQTTClient.publish(MQTT_WILL_TOPIC,MQTT_WILL_ONLINE,1);		// once connected, update status of will topic
	}
	// Report MQTT status
	DEBUG_PRINT(F("MQTT connection state: "));DEBUG_PRINTLN(MQTTClient.state());

	return MQTTClient.connected();
}

/**
 * OTA
 */
 

void SetupOTA() {

	  // ArduinoOTA.setPort(8266);							// Port defaults to 8266
	  ArduinoOTA.setHostname(HOSTNAME);						// Hostname defaults to esp8266-[ChipID]

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
 * HTTP server configuration
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
		
		String htmlMessage = "";
		// Page start
		htmlMessage += FPSTR(htmlStart);
		
		// Header + menu
		htmlMessage += FPSTR(htmlMenu);
		htmlMessage += "<script>\r\n";
		if (!MQTTClient.connected()) {
			htmlMessage += "window.onload = function() { document.getElementById('menunotification').innerHTML = 'Warning: MQTT not connected !';};\r\n"; };
		htmlMessage += "document.getElementById('menuhome').classList.add('active');</script>";

		
		// Live data
		htmlMessage += "<h3>RFLink Live Data *</h3>\r\n";

		htmlMessage += "<input type=\"button\" value =\"Pause\" onclick=\"stopUpdate();\" />";			// Pause
		htmlMessage += "<input type=\"button\" value =\"Restart\" onclick=\"restartUpdate();\" />";		// Restart
		htmlMessage += "<table id=\"liveData\" class='multirow multirow-left';>\r\n";					// Table of x lines
		htmlMessage += "<tr class=\"header\"><th class='t-left'>Raw Data</th><th class='t-left'> MQTT Topic </th><th class='t-left'> MQTT JSON </th></tr>\r\n";
		for (int i = 0; i < (7); i++){
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
		htmlMessage += "for (i = (7-1); i> 0; i--) {\r\n";
		htmlMessage += "document.getElementById('data'+i).innerHTML = document.getElementById('data'+(i-1)).innerHTML;\r\n";
		htmlMessage += "}";
		/*
		for (int i = (7 - 1); i > 0; i--){
			htmlMessage += "document.getElementById(\"data" + String(i) + "\").innerHTML = document.getElementById(\"data" + String (i-1) + "\").innerHTML;\r\n";
		}*/
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
		htmlMessage += "<div class='note'>* see Live Data tab for more lines - web view may not catch all frames, MQTT debug is more accurate</div>\r\n";
		// Commands to RFLink
		htmlMessage += "<h3>Commands to RFLink</h3><br />";
		htmlMessage += "<form action=\"/send\" id=\"form_command\" style=\"float: left;\"><input type=\"text\" size=\"32\" id=\"command\" name=\"command\">";
		htmlMessage += "<input type=\"submit\" value=\"Send\"><a class='button help' href='http://www.rflink.nl/blog2/protref' target='_blank'>&#10068;</a></form>\r\n";
		htmlMessage += "<script>function sF(cmd) {"
			"document.getElementById('command').value = cmd;"
			"var url = '/send';"
			"var formData = new FormData();"
			"formData.append(\"command\", cmd);"
			"var fetchOptions = {"
			"  method: 'POST',"
			"  headers : new Headers(),"
			"  body: formData"
			"};"
			"fetch(url, fetchOptions);"
			"return false;"
			"}</script>";
		htmlMessage += "<div id='cmds'>\r\n";
		for (int i = 0; i < (int) (sizeof(user_cmds) / sizeof(user_cmds[0])); i++){		// User commands defined in user_cmds for quick access
			htmlMessage += "<a class='button link' href=\"javascript:void(0)\" "; 
			htmlMessage += "onclick=\"sF('" + String(user_cmds[i][1]) + "');\">" + String(user_cmds[i][0]) + "</a>\r\n";  
		}
		htmlMessage += "</div>\r\n";
		htmlMessage += "<a id=\"filtered_ids\"/></a><br>\r\n";
		htmlMessage += "<br style=\"clear: both;\"/>\r\n";
		
		// Filtered IDs
		if (eepromConfig.id_filtering) {												// show list of filtered IDS
			int id_to_show_number = filtered_id_number;
			for (int i = 0; i < filtered_id_number; i++){
				if (strcmp(eepromConfig.filtered_id[i].id,"") == 0) {
					id_to_show_number = i;
					break;
				}
			}
			htmlMessage += "<h3 id='configuration'>ID filtering * \r\n";
			if (id_to_show_number > 16) {
				for (int i = 0; i <= ((int) (id_to_show_number-1)/16); i++){
					htmlMessage += "<a href='/?page=" + String(i) + "#filtered_ids' class='button link'>" + String(i*16+1) + " - " + String(min(id_to_show_number,i*16+16)) + "</a> | \r\n";
				}	
			}
			htmlMessage += "</h3>\r\n";
			htmlMessage += "\r\n";
			htmlMessage += "<table class='multirow'><tr><th>#</th><th>ID</th><th>ID applied</th><th>Description</th><th>Interval</th><th>Received</th><th>Published</th><th class='t-left'>Last MQTT JSON</th></tr>\r\n";
			for (int i = 0+(16*page); i < min(id_to_show_number,16*(page+1)); i++){
				htmlMessage += "<tr><td>" + String(i+1) + "</td><td>" + String(eepromConfig.filtered_id[i].id) + "</td>";
				htmlMessage += "<td>" + String(eepromConfig.filtered_id[i].id_applied) + "</td>";
				htmlMessage += "<td class='t-left'>" + String(eepromConfig.filtered_id[i].description) + "</td>";
				(eepromConfig.filtered_id[i].publish_interval <= 60000) ? htmlMessage += "<td>" + String( int(float(eepromConfig.filtered_id[i].publish_interval) *0.001)) + " s</td>"	 :	htmlMessage += "<td>" + String( int(float(eepromConfig.filtered_id[i].publish_interval) *0.001 /60)) + " min</td>";
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
			htmlMessage += "</table>\r\n";
			htmlMessage += "<div class='note'>* only the above IDs are published on MQTT server, and if data changed or interval time was exceeded</div><br>\r\n";
			htmlMessage += "<a href='/disable-id_filtering' class='button link'>Disable ID filtering</a> \r\n";
			htmlMessage += "<a class='button link' href='/configuration'>Configure ID filtering</a>\r\n";
			
		} else {
			htmlMessage += "<h3 id='configuration'>No ID filtering *</h3>\r\n";
			htmlMessage += "<div class='note'>* all received messages are forwarded to MQTT server</div><br><a href='/enable-id_filtering' class='button link' style='font-weight:normal'>Enable ID filtering</a>\r\n";
		}
		htmlMessage += "<br>\r\n";
		
		// Page end
		htmlMessage += FPSTR(htmlEnd);
		
		DEBUG_PRINT("Free mem: ");DEBUG_PRINTLN(ESP.getFreeHeap());
		httpserver.send(200, "text/html", htmlMessage);
	}); 


	httpserver.on("/livedata",[](){       // 
		DEBUG_PRINTLN("Html page requested: /live-data");
		
		String htmlMessage = "";
		// Page start
		htmlMessage += FPSTR(htmlStart);
		
		// Header + menu
		htmlMessage += FPSTR(htmlMenu);
		htmlMessage += "<script>\r\n";
		if (!MQTTClient.connected()) {
			htmlMessage += "window.onload = function() { document.getElementById('menunotification').innerHTML = 'Warning: MQTT not connected !';};\r\n"; };
		htmlMessage += "document.getElementById('menulivedata').classList.add('active');</script>";
		
		// Live data
		htmlMessage += "<h3>RFLink Live Data</h3>\r\n";

		htmlMessage += "<input type=\"button\" value =\"Pause\" onclick=\"stopUpdate();\" />";			// Pause
		htmlMessage += "<input type=\"button\" value =\"Restart\" onclick=\"restartUpdate();\" />";		// Restart
		htmlMessage += "<input type=\"text\" id=\"mySearch\" onkeyup=\"filterLines()\" placeholder=\"Search for...\" title=\"Type in a name\"><br />\r\n";    // Search
		
		htmlMessage += "<table id=\"liveData\" class='multirow multirow-left';>\r\n";					 // Table of x lines
		htmlMessage += "<tr class=\"header\"><th class='t-left'> <a onclick='sortTable(0)'>Time</a></th><th class='t-left'> <a onclick='sortTable(1)'>Raw Data</a> </th><th class='t-left'> <a onclick='sortTable(2)'>MQTT Topic</a> </th><th class='t-left'> <a onclick='sortTable(3)'>MQTT JSON</a> </th></tr>\r\n";
		htmlMessage += "<tr id=\"data0" "\"><td></td><td></td><td></td><td></td></tr>\r\n";
		htmlMessage += "</table>\r\n";
		htmlMessage += "<script>\r\n";                                                      	// Script to filter lines
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
		httpserver.send(200, "text/html", "Rebooting ESP...");
		  delay(500);
		ESP.restart();
		//ESP.reset();
	});

	httpserver.on("/reset-mega",[](){                             // Reset MEGA
		DEBUG_PRINTLN("Html page requested: /reset-mega");
		
		DEBUG_PRINTLN("Resetting Mega...");
		httpserver.send(200, "text/html", "Resetting Mega...");
		#if defined(MQTT_MEGA_RESET_TOPIC)
			MQTTClient.publish(MQTT_MEGA_RESET_TOPIC,"1",MQTT_RETAIN_FLAG);
			delay(1000);
			MQTTClient.publish(MQTT_MEGA_RESET_TOPIC,"0",MQTT_RETAIN_FLAG);
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
		MQTTClient.publish(MQTT_DEBUG_TOPIC,"{\"DATA\":\" \",\"ID\":\" \",\"NAME\":\" \",\"TOPIC\":\" \",\"JSON\":\" \"}",1);
		httpserver.sendHeader("Location","/infos");
		httpserver.send(303);
	});

	httpserver.on("/enable-id_filtering",[](){
		DEBUG_PRINTLN("Enabling ID filtering...");
		eepromConfig.id_filtering = 1;
		saveEEPROM();
		httpserver.sendHeader("Location","/#filtered_ids");
		httpserver.send(303);
	});
	httpserver.on("/disable-id_filtering",[](){
		DEBUG_PRINTLN("Disabling ID filtering...");
		eepromConfig.id_filtering = 0;
		saveEEPROM();
		httpserver.sendHeader("Location","/#filtered_ids");
		httpserver.send(303);
		});

	httpserver.on("/data.txt", [](){			// Used to deliver raw data received (BUFFER) and mqtt data published (MQTT_TOPIC and JSON)           
		httpserver.send(200, "text/html","<td>" + String(BUFFER) + "</td><td>" + String(MQTT_TOPIC) + "</td><td>" + String(JSON) + "</td>\r\n");
	});

	httpserver.on("/wifi-scan", [](){
		DEBUG_PRINTLN("Html page requested: /wifi-scan");
		int numberOfNetworks = WiFi.scanNetworks();
		String htmlMessage = "";
		htmlMessage += "<table>\r\n";
		for(int i =0; i<numberOfNetworks; i++){
		htmlMessage += "<tr><td>" + String(i+1) + " - " + String(WiFi.SSID(i)) + " </td><td> " + String(WiFi.RSSI(i)) + " dBm </td><tr>\r\n";
		}		
		htmlMessage += "</table>\r\n";
		httpserver.send(200, "text/html", htmlMessage);
	});

	httpserver.on("/infos", [](){
		DEBUG_PRINTLN("Html page requested: /infos");
		
		String htmlMessage = "";
		// Page start
		htmlMessage += FPSTR(htmlStart);
		
		// Header + menu
		htmlMessage += FPSTR(htmlMenu);
		htmlMessage += "<script>\r\n";
		if (!MQTTClient.connected()) {
			htmlMessage += "window.onload = function() { document.getElementById('menunotification').innerHTML = 'Warning: MQTT not connected !';};\r\n"; };
		htmlMessage += "document.getElementById('menuinfos').classList.add('active');</script>";

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
		htmlMessage += "<tr><td>WiFi network</td><td>" + String(WiFi.SSID()) + " " + WiFi.RSSI() + " dBm | <a href='/wifi-scan' class='button link' onclick=\"fetchAndNotify('/wifi-scan');return false;\">Scan</a>";
		#ifdef ENABLE_WIFI_SETTINGS_ONLINE_CHANGE
			//htmlMessage += " | <a class='button link' href='#' onclick=\"document.getElementById('wificonfigure').style.display = 'block';return false;\">Configure</a>";
			htmlMessage += "<form method='post' action='/update-settings'><table class='condensed'id='wificonfigure'><input type='hidden' name='save_wifi' value='1'>\r\n";
			htmlMessage += "<tr><td>ssid</td><td><input type='text' name='ssid' maxlength='" + String(CFG_SSID_SIZE) + "' value='" + String(eepromConfig.ssid) + "'></td></tr>\r\n";
			htmlMessage += "<tr><td>password</td><td><input type='password' name='psk' maxlength='" + String(CFG_PSK_SIZE) + "' value='" + String(eepromConfig.psk) + "'></td></tr>\r\n";
			htmlMessage += "<tr><td>hostname</td><td><input type='text' name='hostname' maxlength='" + String(CFG_HOSTNAME_SIZE) + "' value='" + String(eepromConfig.hostname) + "'></td></tr>\r\n";
			htmlMessage += "<tr><td></td><td><input type='submit' value='Apply'>*reboot required</td></tr></table></form>\r\n";
		#endif
		//htmlMessage += "<div style='margin-top:0.25em;'id='wifiscan'></div>\r\n";
		htmlMessage += "\r\n</td></tr>\r\n";
		htmlMessage += "<tr><td>IP address (MAC)</td><td>" + WiFi.localIP().toString() +" (" + String(WiFi.macAddress()) +")</td></tr>\r\n";
		htmlMessage += "<tr><td>ESP pin to reset MEGA</td><td>GPIO " + String(MEGA_RESET_PIN);
		if (resetMegaInterval > 0) {
			htmlMessage += " - auto reset after " + String( int(float(resetMegaInterval) *0.001 /60)) + " min if no data received - last data " + time_string_exp(now - lastReceived) + " ago";// - " + resetMegaCounter + " resets since ESP reboot";
		}
		htmlMessage += "</td></tr>";
		#ifdef ENABLE_MQTT_SETTINGS_ONLINE_CHANGE
			htmlMessage += "<tr><td>MQTT configuration</td><td><form method='post' action='/update-settings'><table class='condensed'><input type='hidden' name='save_mqtt' value='1'>\r\n";
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
		htmlMessage += "<tr><td>uptime (min, every " + String( int( float(UPTIME_INTERVAL) *0.001 / 60) ) + ")</td><td> " + String(MQTT_UPTIME_TOPIC) + "</td></tr>\r\n";
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
		htmlMessage += "<h3>Firmware and memory</h3>\r\n";
		htmlMessage += "<table class='normal high'>\r\n";
		htmlMessage += "<td style='min-width:150px;'><a href='/update' class='button link'>Load&nbsp;firmware</a></td><td style='width:80%;'>Load a new firmware for the ESP</td>\r\n";
		#ifdef ENABLE_SERIAL_DEBUG
			htmlMessage += "<tr><td><a href='/read-eeprom' class='button link' onclick=\"fetchAndNotify('/read-eeprom');return false;\">Read&nbsp;EEPROM</a></td><td>Output EEPROM content to serial debug</td></tr>\r\n";
		#endif
		htmlMessage += "<tr><td><a href='/erase-eeprom' class='button link' onclick=\"fetchAndNotify('/erase-eeprom');return false;\">Erase&nbsp;EEPROM</a></td><td>Delete WiFi settings, MQTT settings, ID filtering configuration<br>Restore default values from firmware</td><tr>\r\n";
		#ifdef EXPERIMENTAL
			htmlMessage += "<tr><td>RFLink packet lost</td><td>" + String (lost_packets) + "</td></tr>\r\n"; // TEST packet lost
		#endif
		htmlMessage += "<tr><td> Compile date</td><td>" + String (__DATE__ " " __TIME__) + "</td></tr>\r\n";
		htmlMessage += "<tr><td> Free Mem</td><td>" + String (ESP.getFreeHeap()) + " K</td></tr>\r\n";
		htmlMessage += "<tr><td> Heap Max Free Block</td><td>" + String(ESP.getMaxFreeBlockSize()) + " K</td></tr>\r\n"; 
		htmlMessage += "<tr><td> Heap Fragmentation</td><td>" + String (ESP.getHeapFragmentation()) + "%</td></tr>\r\n";
		htmlMessage += "</table>\r\n";

		// Page end
		htmlMessage += FPSTR(htmlEnd);

		httpserver.send(200, "text/html", htmlMessage);
	});

	httpserver.on("/update-settings", [](){					// Used to change settings in EEPROM
		DEBUG_PRINTLN("Html page requested: /update-settings");

		// URL arguments

		#ifdef ENABLE_WIFI_SETTINGS_ONLINE_CHANGE
			if (httpserver.hasArg("save_wifi")) {
				strncpy(eepromConfig.ssid 			,   httpserver.arg("ssid").c_str(),    		 	CFG_SSID_SIZE);
				strncpy(eepromConfig.psk 			,   httpserver.arg("psk").c_str(),     			CFG_PSK_SIZE);
				strncpy(eepromConfig.hostname		,   httpserver.arg("hostname").c_str(),    		CFG_HOSTNAME_SIZE);
				saveEEPROM();
				DEBUG_PRINTLN("WiFi settings updated.");
			}
		#endif

		#ifdef ENABLE_MQTT_SETTINGS_ONLINE_CHANGE
			if (httpserver.hasArg("save_mqtt")) {
				int itemp;
				strncpy(eepromConfig.mqtt.server 	,   httpserver.arg("mqtt_server").c_str(),     	CFG_MQTT_SERVER_SIZE);
				itemp = httpserver.arg("mqtt_port").toInt();
				eepromConfig.mqtt.port = (itemp>0 && itemp<=65535) ? itemp : MQTT_PORT;
				strncpy(eepromConfig.mqtt.user 		,   httpserver.arg("mqtt_user").c_str(),     	CFG_MQTT_USER_SIZE);
				strncpy(eepromConfig.mqtt.password	,   httpserver.arg("mqtt_password").c_str(),    CFG_MQTT_PASSWORD_SIZE);
				saveEEPROM();
				DEBUG_PRINTLN("MQTT settings updated.");
			}
		#endif

		DEBUG_PRINTLN("New EEPROM configuration:");
		loadEEPROM();
		showEEPROM();
		httpserver.sendHeader("Location","/infos");
		httpserver.send(303);

	});

	httpserver.on("/configuration", [](){						// Used to change filtered_IDs configuration in EEPROM
		DEBUG_PRINTLN("Html page requested: /configuration");
		
		// URL arguments
		int page = 0;
		String arg_page = httpserver.arg("page");
		if (arg_page.length() > 0) {
			page = max((int) arg_page.toInt(),0);
		};
		if (httpserver.hasArg("save_configuration")) {
			for (int i = 0+(16*page); i < min(filtered_id_number,16*(page+1)); i++){
				//String arg_line = httpserver.arg("line["+String(i)+"]"	);
				String arg_id = httpserver.arg("id["+String(i)+"]");
				String arg_id_applied = httpserver.arg("id_a["+String(i)+"]");
				String arg_description = httpserver.arg("d["+String(i)+"]");
				String arg_publish_interval = httpserver.arg("pi["+String(i)+"]");
				DEBUG_PRINTLN(arg_id + " - " + arg_id_applied + " - " + arg_description + " - " + arg_publish_interval);
				arg_id.toCharArray(eepromConfig.filtered_id[i].id,MAX_ID_LEN+1);
				arg_id_applied.toCharArray(eepromConfig.filtered_id[i].id_applied,MAX_ID_LEN+1);
				arg_description.toCharArray(eepromConfig.filtered_id[i].description,MAX_DATA_LEN+1);
				eepromConfig.filtered_id[i].publish_interval = max((long) 0,arg_publish_interval.toInt()*1000);
			}
		}

		String htmlMessage = "";
		// Page start
		htmlMessage += FPSTR(htmlStart);

		// Header + menu
		htmlMessage += FPSTR(htmlMenu);

		if (eepromConfig.id_filtering) {						// show list of filtered IDS
			htmlMessage += "<h3>ID filtering configuration\r\n";
			if (filtered_id_number > 16) {
				for (int i = 0; i <= ((int) (filtered_id_number-1)/16); i++){
					htmlMessage += "<a href='/configuration?page=" + String(i) + "' class='button link'>" + String(i*16+1) + " - " + String(min(filtered_id_number,i*16+16)) + "</a>\r\n";
				}
				htmlMessage += "</h3><br>\r\n";
			}
			htmlMessage += "<div class='note'>Make changes in the table below and click on \"Apply configuration\". It can be tested immediately.<br>If OK, save configuration to permanent memory by clicking on \"Save to EEPROM\".<br>Tip: Leave an empty ID after last device. ID filtering comparison will stop there.</div><br>\r\n";

			htmlMessage += "<form method='post' action='/configuration'><input type='hidden' name='page' value=" + String(page) + ">\r\n";
			htmlMessage += "<input type='hidden' name='save_configuration' value='1'>";
			htmlMessage += "<table class='multirow' id='configuration_table'><tr><th>#</th><th>ID</th><th>ID applied</th><th>Description</th><th>Publish Interval (s)  </th><th></th></tr>\r\n";
			for (int i = 0+(16*page); i < min(filtered_id_number,16*(page+1)); i++){
				htmlMessage += "<tr>";
				htmlMessage += "<td>" + String(i+1) + "</td>";
				htmlMessage += "<td><input type='text' name='id[" + String(i) + "]' maxlength='" + String(MAX_ID_LEN) + "' value='" + String(eepromConfig.filtered_id[i].id) + "'></td>";
				htmlMessage += "<td><input type='text' name='id_a[" + String(i) + "]' maxlength='" + String(MAX_ID_LEN) + "' value='" + String(eepromConfig.filtered_id[i].id_applied) + "'></td>";
				htmlMessage += "<td><input type='text' name='d[" + String(i) + "]' maxlength='" + String(MAX_DATA_LEN) + "' value='" + String(eepromConfig.filtered_id[i].description) + "'></td>";
				htmlMessage += "<td><input type='number' name='pi[" + String(i) + "]' step='1' min='0' value='" + String((int) eepromConfig.filtered_id[i].publish_interval/1000) + "'></td>";
				htmlMessage += "<td></td></tr>\r\n";
			}
			htmlMessage += "</table><br><input type='submit' value='Apply configuration'></form><br><br>\r\n";
			htmlMessage += "<a class='button link' href='/save-eeprom' onclick=\"fetchAndNotify('/save-eeprom');return false;\">Save to EEPROM</a> => Save configuration to persistent memory, or changes will be lost on next reboot";  
			htmlMessage += "<br><br><br><div style='font-size:0.8em;'>In case you compile your own firmware, you can update easily config.h file with current configuration using <a style='font-size:0.8em;' href='/config_h_code'>this code</a><div><br>\r\n";
		} else {
			htmlMessage += "<h3>ID filtering is disabled</h3><br>\r\n";
		}
		
		// Page end
		htmlMessage += FPSTR(htmlEnd);
		
		DEBUG_PRINT("Free mem: ");DEBUG_PRINTLN(ESP.getFreeHeap());
		httpserver.send(200, "text/html", htmlMessage);
	});
	

	httpserver.on("/config_h_code",[](){									// Show code to update config.h
		DEBUG_PRINTLN("Html page requested: /config_h_code");
		String htmlMessage = "<xmp id='code_config_h' style='font-size:0.9em;'>\r\n";
		for (int i = 0; i < (filtered_id_number); i++){
			String str_description = String(eepromConfig.filtered_id[i].description);     
			htmlMessage += "	{\"" + String(eepromConfig.filtered_id[i].id) + "\",\"" + String(eepromConfig.filtered_id[i].id_applied) + "\"," + String(eepromConfig.filtered_id[i].publish_interval) + ",\"" + str_description + "\"},	// " + (i+1) + "\r\n";
		}
		htmlMessage += "</xmp>\r\n";
		httpserver.send(200, "text/html", htmlMessage);
	});

	httpserver.on("/save-eeprom",[](){									// Saving EEPROM + reboot
		DEBUG_PRINTLN("Html page requested: /save-eeprom");
		DEBUG_PRINTLN("Saving EEPROM");
		saveEEPROM();
		DEBUG_PRINTLN("Rebooting device...");
		httpserver.send(200, "text/html", "EEPROM saved, rebooting...");
		delay(500);
		ESP.restart();
		//ESP.reset();
	});

	httpserver.on("/erase-eeprom",[](){									// Erasing EEPROM + reboot
		DEBUG_PRINTLN("Html page requested: /erase-eeprom");
		DEBUG_PRINTLN("Erasing EEPROM configuration");
		DEBUG_PRINT("EEPROM config max size: ");DEBUG_PRINTLN(eepromConfigMaxSize);
		EEPROM.begin(4096);
		for (int i = eepromAddress; i < (eepromConfigMaxSize); i++) {
			EEPROM.write(i, 0);
		}
		EEPROM.end();
		DEBUG_PRINTLN("Rebooting device...");
		httpserver.send(200, "text/html", "EEPROM configuration erased, rebooting...");
		delay(500);
		ESP.restart();
		//ESP.reset();
	});

	#ifdef ENABLE_SERIAL_DEBUG
	httpserver.on("/read-eeprom",[](){								// Erasing EEPROM + reboot
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
		httpserver.send(200, "text/html", "EEPROM read to serial");
		//httpserver.sendHeader("Location","/infos");
		//httpserver.send(303);
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

String time_string_exp(long time) {
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
	return strUpTime;
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
	
	delay(100);
	
	#ifdef ENABLE_SERIAL_DEBUG
		debugSerialTX.begin(57600);        // debug serial : same speed as RFLInk to be compatible if same port used
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

	// Setup WiFi
	#ifdef ENABLE_WIFI_SETTINGS_ONLINE_CHANGE
		setup_complex_wifi(); 
		// Start the HTTP server
		httpserver.begin();
		DEBUG_PRINTLN("HTTP server started");
	#else
		setup_simple_wifi(); 
		// Start the HTTP server
		httpserver.begin();
		DEBUG_PRINTLN("HTTP server started");
	#endif

	DEBUG_PRINT("Size of eepromConfig: ");DEBUG_PRINTLN(sizeof(eepromConfig));
	DEBUG_PRINT("Size of filtered_IDs: ");DEBUG_PRINTLN(sizeof(filtered_IDs));
	DEBUG_PRINT("Size of matrix: ");DEBUG_PRINTLN(sizeof(matrix));

	rflinkSerialTX.println();
	delay(1000);
	rflinkSerialTX.println(F("10;ping;"));                        // Do a PING on startup
	delay(500);
	rflinkSerialTX.println(F("10;version;"));                     // Ask version to RFLink

} // setup

//********************************************************************************
// Main loop
//********************************************************************************

void loop() {

	bool DataReady=false;
	now = millis();

	// handle lost of connection : retry after 10s on each loop
	if (!MQTTClient.connected()) {
		
		if (now - lastMqttConnect >= MQTT_CHECK_INTERVAL) {
			DEBUG_PRINT("MQTT not connected");
			if (mqttConnectionAttempts == 0) {
				DEBUG_PRINT(" retrying after ");DEBUG_PRINT(MQTT_CHECK_INTERVAL/1000);DEBUG_PRINTLN(" s");
				}
			lastMqttConnect = now;
			mqttConnect();
		}
	}// else {
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
			strcpy(MQTT_ID,""); strcpy(MQTT_NAME,"");strcpy(MQTT_TOPIC,"");strcpy(JSON,"");

			// read data
			readRfLinkPacket(BUFFER);

			#ifdef EXPERIMENTAL
				// check if a line was lost TEST
				//DEBUG_PRINTLN(LINE_NUMBER);
				byte expected_line_number = line_number + 1;			//DEBUG_PRINTLN(expected_line_number);
				line_number = strtoul(LINE_NUMBER,NULL,16);				//DEBUG_PRINTLN(line_number);
				if (lost_packets == -1) {
					lost_packets = 0;									//DEBUG_PRINTLN(lost_packets);
				} else {
					lost_packets += line_number - expected_line_number;	//DEBUG_PRINTLN(lost_packets);
					//MQTTClient.publish("rflink/lost_packets",(char*) lost_packets);
				}
			#endif

			// Store last received data time if MQTT_ID is valid
			if ( (strcmp(MQTT_ID,"") != 0) && (strcmp(MQTT_ID,"0\0") != 0) ) lastReceived = millis();
			
			// If user_specific_ids is used
			for (int i = 0; i < (int) (sizeof(user_specific_ids) / sizeof(user_specific_ids[0])); i++){
				if (strcmp(MQTT_NAME,user_specific_ids[i][0]) == 0){		// check if protocol / name matches
				if (strcmp(user_specific_ids[i][1],"ALL") == 0) {			// applies new ID to all IDs for specific name/protocol
					strcpy( MQTT_ID , user_specific_ids[i][2]);
				} else if (strcmp(user_specific_ids[i][1],MQTT_ID) == 0) {
					strcpy( MQTT_ID , user_specific_ids[i][2]);				// applies new ID to specific ID for specific name/protocol
				}
			}
			}
			
			// construct topic name to publish to
			buildMqttTopic();
			
			// report message for debugging
			printToSerial();
			
			// MQTT debug mode with json, full data (published in several times to avoid changing MQTT_MAX_PACKET_SIZE in PubSubClient.h)
			if (MQTT_DEBUG) {
				char BUFFER_DEBUG [10 + BUFFER_SIZE + 10 + MAX_ID_LEN + 10 + MAX_DATA_LEN + 10];
				char JSON_DEBUG   [BUFFER_SIZE];
				strcpy(BUFFER_DEBUG,"{");
				strcat(BUFFER_DEBUG,"\"ID\":\"");strcat(BUFFER_DEBUG,MQTT_ID);strcat(BUFFER_DEBUG,"\"");
				strcat(BUFFER_DEBUG,",\"NAME\":\"");strcat(BUFFER_DEBUG,MQTT_NAME);strcat(BUFFER_DEBUG,"\"");
				strcat(BUFFER_DEBUG,",\"TOPIC\":\"");strcat(BUFFER_DEBUG,MQTT_TOPIC);strcat(BUFFER_DEBUG,"\"");
				strcat(BUFFER_DEBUG,"}");
				MQTTClient.publish(MQTT_DEBUG_TOPIC,BUFFER_DEBUG,1);
				strcpy(BUFFER_DEBUG,"{");
				strcpy(JSON_DEBUG,JSON);
				for (char* p = JSON_DEBUG; (p = strchr(p, '\"')) ; ++p) {*p = '\'';}	// Remove quotes
				strcat(BUFFER_DEBUG,"\"JSON\":\"");strcat(BUFFER_DEBUG,JSON_DEBUG);strcat(BUFFER_DEBUG,"\"");
				strcat(BUFFER_DEBUG,",\"ID\":\"");strcat(BUFFER_DEBUG,MQTT_ID);strcat(BUFFER_DEBUG,"\"");
				strcat(BUFFER_DEBUG,"}");
				MQTTClient.publish(MQTT_DEBUG_TOPIC,BUFFER_DEBUG,1);
				strcpy(BUFFER_DEBUG,"{");
				strcat(BUFFER_DEBUG,"\"DATA\":\"");strncat(BUFFER_DEBUG,BUFFER,strlen(BUFFER)-2);strcat(BUFFER_DEBUG,"\"");
				strcat(BUFFER_DEBUG,",\"ID\":\"");strcat(BUFFER_DEBUG,MQTT_ID);strcat(BUFFER_DEBUG,"\"");
				strcat(BUFFER_DEBUG,"}");
				MQTTClient.publish(MQTT_DEBUG_TOPIC,BUFFER_DEBUG,1);
				}
			
			// MQTT debug mode, raw data only	
			//if (MQTT_DEBUG) MQTTClient.publish(MQTT_DEBUG_TOPIC,BUFFER);
			
			// ID filtering with filtered_IDs 
			if (eepromConfig.id_filtering) {
				for (int i = 0; i < (filtered_id_number); i++){
					DEBUG_PRINT("ID = ");DEBUG_PRINT(eepromConfig.filtered_id[i].id);
					if (strcmp(eepromConfig.filtered_id[i].id,"") == 0) {				// exits if empty MQTT_ID
						DEBUG_PRINTLN("Empty ID in configuration, exit ID filtering loop");
						break;
					}
					DEBUG_PRINT(" ; ID_applied = ");DEBUG_PRINT(eepromConfig.filtered_id[i].id_applied);
					DEBUG_PRINT(" ; publish_interval = ");DEBUG_PRINT(eepromConfig.filtered_id[i].publish_interval);
					DEBUG_PRINT(" ; decription = ");DEBUG_PRINT(eepromConfig.filtered_id[i].description);
					DEBUG_PRINT(" ; json = ");DEBUG_PRINT(matrix[i].json);
					//DEBUG_PRINT(" ; json_checksum = ");DEBUG_PRINT(matrix[i].json_checksum);
					DEBUG_PRINT(" ; last_published = ");DEBUG_PRINT(matrix[i].last_published);
					DEBUG_PRINT(" ; last_received = ");DEBUG_PRINTLN(matrix[i].last_received);

					// check ID is authorized
					if (strcmp(MQTT_ID,eepromConfig.filtered_id[i].id) == 0) {
						DEBUG_PRINT("Authorized ID ");DEBUG_PRINT(MQTT_ID);
						matrix[i].last_received = millis();									// memorize received time
						if (strcmp(eepromConfig.filtered_id[i].id_applied,"") != 0 ) {		// apply different ID if available
							strcpy(MQTT_ID,eepromConfig.filtered_id[i].id_applied);
							buildMqttTopic();													// rebuild MQTT_TOPIC
							}
						//uint16_t json_checksum = crc16_ccitt((byte*) &JSON, sizeof(JSON));
						// check if json data has changed or it was never publised
						if ( (strncmp(matrix[i].json,JSON,sizeof(matrix[i].json)-2) != 0) || (!matrix[i].last_published) ) {
							//if (matrix[i].json_checksum != json_checksum) {					// check if json_checksum has changed
							DEBUG_PRINT(" => data changed or never published => published on ");DEBUG_PRINTLN(MQTT_TOPIC);
							if (MQTTClient.publish(MQTT_TOPIC,JSON,MQTT_RETAIN_FLAG)) {			// data changed, publish on MQTT server
								matrix[i].last_published = millis();							// memorize published time
							} else {
								DEBUG_PRINTLN(" => MQTT publish failed");
							};	
							strncpy(matrix[i].json,JSON,sizeof(matrix[i].json)-2);				// memorize new json value
							//matrix[i].json_checksum = json_checksum;							// memorize new json_checksum
						} else {																// no data change
							if ( (now - matrix[i].last_published) > (eepromConfig.filtered_id[i].publish_interval) ) {	// check if it exceeded time for last publish
								DEBUG_PRINT(" => no data change but max time interval exceeded => published on ");
								DEBUG_PRINTLN(MQTT_TOPIC);
								if (MQTTClient.publish(MQTT_TOPIC,JSON,MQTT_RETAIN_FLAG)) {			// publish on MQTT server
									matrix[i].last_published = millis();							// memorize published time
								} else {
									DEBUG_PRINTLN(" => MQTT publish failed");
								}
							} else {
								DEBUG_PRINTLN(" => no data change => not published");
							}
						}		// no data changed
						break;
					}	// authorized id
				}	// for
			} else {	// No ID filtering
				if (MQTTClient.publish(MQTT_TOPIC,JSON,MQTT_RETAIN_FLAG)) {							// publish on MQTT server
				} else {
					DEBUG_PRINTLN(" => MQTT publish failed");
				};
			}
			
		} // end of DataReady

		// Handle uptime
		if ( (now - lastUptime) > (UPTIME_INTERVAL) ) {						// if uptime interval is exceeded
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
				DEBUG_PRINT("No data received for ");DEBUG_PRINT(time_string_exp(now - lastReceived));
				DEBUG_PRINTLN(": Resetting Mega");
				#if defined(MQTT_MEGA_RESET_TOPIC)
				  MQTTClient.publish(MQTT_MEGA_RESET_TOPIC,"1",MQTT_RETAIN_FLAG);
				  delay(1000);
				  MQTTClient.publish(MQTT_MEGA_RESET_TOPIC,"0",MQTT_RETAIN_FLAG);
				#endif
				delay(200);
				pinMode(MEGA_RESET_PIN, OUTPUT);
				delay(200);
				digitalWrite(MEGA_RESET_PIN,false);					// Change the state of pin to ground
				delay(1000); 
				digitalWrite(MEGA_RESET_PIN,true);					// Change the state of pin to VCC
				delay(50);
				lastReceived = millis();							// Fake the last received time to avoid permanent reset
			}
		}

		// Handle MQTT callback
		MQTTClient.loop();

	//} // else (=connected on MQTT server)

	ArduinoOTA.handle();						// Listen for OTA update
	httpserver.handleClient();					// Listen for HTTP requests from clients

} // loop
