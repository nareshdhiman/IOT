/*
 * Basic routines for building programs for Linkit One or Arduino board.
 * 
 * Program contains following basic rountines:
 * display - Displays the text on supported 128x64 oled display screen.
 * console - Prints the text on serial monitor.
 * readConfig - Reads configuration from config file on flash.
 * wifiInit - Initializes Wifi for communicating with Internet.
 * mqttInit - Initializes MQTT for publishing and subscribing for events.
 * .
 * Author: Naresh Dhiman
 * .
 * Bugs/Enhancments, I like to address:-
 * (1) Reconnect Wifi if Wifi gets disconnnected -- FIXED
 * (2) Reconnect MQTT if MQTT server gets disconnected -- FIXED
 * (3) Support dotted IP Address config for MQTT server -- FIXED
 * (4) Display buffer contains hello world
 * (5) Validate MQTT Server IP address.
 * 
 */

/* Display routine initialization*/
#include <Wire.h>
#include <SeeedOLED.h>
#define ND_PM 2
#define ND_PM_CLR 3
#define ND_HM 6
#define ND_HM_CLR 7

/* Console routine initialization*/
#define NEW_LINE 1
#define NO_NEW_LINE 0

/* WIFI routine initialization*/
#include <LWiFi.h>
#include <LWiFiClient.h>

/* MQTT routine initialization*/
#include <PubSubClient.h>

/* File Config */
#include <LFlash.h>
#include <LStorage.h>
#define CONFIG_FILE "arduino_basic_routines.config"

char buff[256];
char* toCArray(String str) {
  str.toCharArray(buff, 256);
  return buff;
}

//-----display routine -------S//
char display_init = 0;
char display_page = 0;
// ctrl(8bits);
// b1=1 clear screen; b2=1 -Normal display, b2=0 -invert display
// b3:1 -horizontal mode, b3:0 -pagemode
void display(unsigned char ctrl, const char * str) {
  if (display_init == 0) {
    Wire.begin();
    SeeedOled.init();
    display_init = 1;
    console(NEW_LINE, "Display initialized.");
  }

  if (ctrl & 1) SeeedOled.clearDisplay();
  if (ctrl & 2) {
    SeeedOled.setNormalDisplay();
  } else {
    SeeedOled.setInverseDisplay();
  }
  if (ctrl & 4) {
    SeeedOled.setHorizontalMode();
  } else {
    if (display_page > 8) display_page = 0;
    SeeedOled.setPageMode();
    SeeedOled.setTextXY(display_page++, 0);
  }
  SeeedOled.putString(str);
}
//-----display routine -------S//

//-----console routine -------S//
char console_init = 0;
//ctrl=0 - print in same line, ctrl=1 - print in new line
void console(unsigned char ctrl, const char *str) {
  if (console_init == 0) {
    Serial.begin(9600);
    console_init = 1;
    Serial.println("Serial initialized.");
  }

  if (ctrl == NO_NEW_LINE) {
    Serial.print(str);
  } else {
    Serial.println(str);
  }
}
//-----console routine -------E//

//-----Flash read routine -------S//
LFile file;
String wifiAP, wifiPassword, wifiAuth;
String mqttServerIP, mqttServerPort, mqttUser, mqttPassword, mqttClientId, mqttTopic;

void parseLine(String line, String& keyValue, const String keyName) {
  line.trim();
  int index = line.indexOf('=');
  if (index != -1) {
    if (line.substring(0, index) == keyName) {
      keyValue = line.substring(index + 1);
      keyValue.trim();
    }
  }
}

int validateKeyValue(const String keyValue, const String keyName) {
  if (keyValue.length() < 1) {
    console(NO_NEW_LINE, "ERROR - invalid config value; ");
    console(NO_NEW_LINE, keyName.c_str());
    console(NO_NEW_LINE, "=");
    console(NEW_LINE, keyValue.c_str());
    return -1;
  } else {
    console(NO_NEW_LINE, keyName.c_str());
    console(NO_NEW_LINE, "=");
    console(NEW_LINE, keyValue.c_str());
    return 0;
  }
}

int readConfig() {
  LFlash.begin();
  console(NEW_LINE, "Reading configuration");
  file = LFlash.open(CONFIG_FILE, FILE_READ);
  if (file) {
    file.seek(0);
    String line;

    while (file.available()) {
      char c = file.read();

      if (c == '\n') {
        parseLine(line, wifiAP, "WIFI_AP");
        parseLine(line, wifiPassword, "WIFI_PASSWORD");
        parseLine(line, wifiAuth, "WIFI_AUTH");
        parseLine(line, mqttServerIP, "MQTT_SERVER_IP");
        parseLine(line, mqttServerPort, "MQTT_SERVER_PORT");
        parseLine(line, mqttUser, "MQTT_USER");
        parseLine(line, mqttPassword, "MQTT_PASSWORD");
        parseLine(line, mqttClientId, "MQTT_CLIENT_ID");
        parseLine(line, mqttTopic, "MQTT_TOPIC");

        //reset the line
        line = "";
      } else {
        line += c;
      }
    }

    //validate
    if (validateKeyValue(wifiAP, "WIFI_AP") == -1) return -1;
    if (validateKeyValue(wifiPassword, "WIFI_PASSWORD") == -1) return -1;
    if (validateKeyValue(wifiAuth, "WIFI_AUTH") == -1) return -1;
    if (validateKeyValue(mqttServerIP, "MQTT_SERVER_IP") == -1) return -1;
    if (validateKeyValue(mqttServerPort, "MQTT_SERVER_PORT") == -1) return -1;
    if (validateKeyValue(mqttUser, "MQTT_USER") == -1) return -1;
    if (validateKeyValue(mqttPassword, "MQTT_PASSWORD") == -1) return -1;
    if (validateKeyValue(mqttClientId, "MQTT_CLIENT_ID") == -1) return -1;
    if (validateKeyValue(mqttTopic, "MQTT_TOPIC") == -1) return -1;

    console(NEW_LINE, "Reading configuration...done");
  } else {
    console(NO_NEW_LINE, "ERROR - File can't be opened - ");
    console(NEW_LINE, CONFIG_FILE);
  }
}

//-----Flash read routine -------E//

//-----WiFi routine -------S//
LWiFiClient c;
char wifi_init = 0;
void wifiInit() {
  if (wifi_init == 0) {
    LWiFi.begin();
    wifi_init = 1;
  }

  if (!c.connected()) {
    console(NO_NEW_LINE, "Connecting to WiFi...");
    LWiFiEncryption enc;
    if(wifiAuth.equalsIgnoreCase("LWIFI_WPA")) enc = LWIFI_WPA;
    if(wifiAuth.equalsIgnoreCase("LWIFI_OPEN")) enc = LWIFI_OPEN;
    if(wifiAuth.equalsIgnoreCase("LWIFI_WEP")) enc = LWIFI_WEP;
    while (0 == LWiFi.connect(wifiAP.c_str(), LWiFiLoginInfo(enc, wifiPassword)))
    {
      delay(1000);
    }
    IPAddress ip = LWiFi.localIP();
    sprintf(buff, "Connected. LocalIP=%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    console(NEW_LINE, buff);

  }
}
//-----WiFi routine -------E//

//-----MQTT routine -------S//
PubSubClient client(c); //client(WiFiClient)
char mqtt_init = 0;

void callback(char* topic, byte * payload, unsigned int length) {
  sprintf(buff, "Message arrived [%s] ", topic);
  console(NO_NEW_LINE, buff);
  for (int i = 0; i < length; i++) {
    sprintf(buff, "%c", (char)payload[i]);
    console(NO_NEW_LINE, buff);
  }
  console(NEW_LINE, "");
  //display on screen for testing
  display(ND_PM_CLR, (char*)payload);
}

void mqttInit() {
  if (mqtt_init == 0) {
    int pDotIndex = -1;
    String octet[4];
    
    int dotIndex = mqttServerIP.indexOf(".");
    octet[0] = mqttServerIP.substring(0,dotIndex);
    
    pDotIndex = dotIndex+1;
    dotIndex = mqttServerIP.indexOf('.', pDotIndex);
    octet[1] = mqttServerIP.substring(pDotIndex, dotIndex);

    pDotIndex = dotIndex+1;
    dotIndex = mqttServerIP.indexOf('.', pDotIndex);
    octet[2] = mqttServerIP.substring(pDotIndex, dotIndex);

    pDotIndex = dotIndex+1;
    dotIndex = mqttServerIP.indexOf('.', pDotIndex);
    octet[3] = mqttServerIP.substring(pDotIndex, dotIndex);

    sprintf(buff,"Parsed MQTT_SERVER_IP=%s %s %s %s", (octet[0]).c_str(), (octet[1]).c_str(), (octet[2]).c_str(), (octet[3]).c_str());
    console(NEW_LINE, buff);
    IPAddress server(octet[0].toInt(), octet[1].toInt(), octet[2].toInt(), octet[3].toInt());
    client.setServer(server, mqttServerPort.toInt());
    client.setCallback(callback);
    mqtt_init = 1;
  }

  // Loop until we're reconnected
  while (!client.connected()) {
    console(NO_NEW_LINE, "Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(mqttClientId.c_str(), mqttUser.c_str(), mqttPassword.c_str())) {
      console(NEW_LINE, "connected");
      client.subscribe(mqttTopic.c_str());
    } else {
      sprintf(buff, "failed, rc=%d, try again in 5 seconds", client.state());
      console(NEW_LINE, buff);
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
int mqttPublish(const char* topic, const char* msg) {
  if (client.connected()) {
    client.publish(topic, msg);
    return 0;
  } else
    return -1;
}
int mqttSubscribe(const char* topic) {
  if (client.connected()) {
    client.subscribe(topic);
    return 0;
  } else
    return -1;
}
int mqttPoll() {
  if (client.connected()) {
    client.loop();
    return 0;
  } else
    return -1;
}
//-----MQTT routine -------E//

/*---------------------------------------
 *---- MAIN setup and loop functions ----
 *---------------------------------------
 */
int configError = 0;
void setup() {
  configError = readConfig();
  if (configError != 0) return;

  wifiInit();
  mqttInit();

}

void loop() {
  if (configError == -1) return;

  /*WiFi not available - start again*/
  wifiInit();


  /*
   * 
   * Write your code here
   * 
   */
   
  /*MQTT poll and reconnect if not connected*/
  if (mqttPoll() == -1) {
    mqttInit();
  }
}


