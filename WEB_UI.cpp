/*
 * PROBLEMS
 * 
 * WEB_UI-----Autoconnect webpage in AP Mode ---> In AP Mode, Config new Ap, open SSID'S these two option is not stable in smartphone, but remaining options in WEB_UI is stable
 * WEB_UI-----Autoconnect webpage in STA Mode ---> In STA Mode, All option is stable in both PC and Smartphone                                       
 * 
 * Reasons for problem:
 * 
 * Maybe different MCU board ( Maybe more compatabile with ESP32 instead of ESP32-S3 )
 * Maybe different version of ESP32 board ( board manager) or AutoConnect Version is not stable 
 * not clear about Reason
 * 
 */


#include "WEB_UI.h"
#include <ESPmDNS.h>
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include "ESPFileServer.h"

extern WebServer server;

const char *SubscribeTopic = "FeedbackP/HOME/#";     // Feedback subscribe
const char *Feedback_publish = "FeedbackS/HOME/#";   // Feedback publishe
#define FIRMWARE_VERSION  "1.1.12-dev"
const char* fw_ver = FIRMWARE_VERSION; 
#include <AutoConnect.h>
AutoConnect Portal(server);
AutoConnectConfig config; 
AutoConnectUpdate update;
AutoConnectAux MQTT_Setting;                                                  // adding the current version of the sketch to the OTA caption.
AutoConnectAux* auxPageMqtt;
AutoConnectAux* SaveMqtt;
AutoConnectAux Sensor_Setting;
AutoConnectAux* auxPageSensor;
AutoConnectAux* SaveSensor;
AutoConnectAux* EspRestart;
AutoConnectAux auxIPConfig;
AutoConnectAux auxRestart;
bool acEnable;

char MQTT::payloadData[256];  // Define static variables

// WiFi and MQTT client initialization
WiFiClientSecure espClient;
PubSubClient mqtt_client(espClient);

// Constructor for MQTT class
MQTT::MQTT(const char *mqtt_broker,  int mqtt_port, const char *mqtt_username, const char *mqtt_password, const char *ca_cert) {
    
    _mqtt_broker = mqtt_broker;
    _mqtt_port = mqtt_port;
    _mqtt_username = mqtt_username;
    _mqtt_password = mqtt_password;
    _ca_cert = ca_cert;
    

    
}

static const char AUX_CONFIGIP[] PROGMEM = R"(
{
  "title": "Config IP",
  "uri": "/configip",
  "menu": true,
  "element": [
    {
      "name": "caption",
      "type": "ACText",
      "value": "<b>Module IP configuration</b>",
      "style": "color:steelblue",
      "posterior": "div"
    },
    {
      "name": "mac",
      "type": "ACText",
      "format": "MAC: %s",
      "style": "font-size:smaller",
      "posterior": "par"
    },
    {
      "name": "staip",
      "type": "ACInput",
      "label": "IP",
      "pattern": "^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$",
      "global": true
    },
    {
      "name": "gateway",
      "type": "ACInput",
      "label": "Gateway",
      "pattern": "^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$",
      "global": true
    },
    {
      "name": "netmask",
      "type": "ACInput",
      "label": "Netmask",
      "pattern": "^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$",
      "global": true
    },
    {
      "name": "dns1",
      "type": "ACInput",
      "label": "DNS1",
      "pattern": "^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$",
      "global": true
    },
    {
      "name": "dns2",
      "type": "ACInput",
      "label": "DNS2",
      "pattern": "^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$",
      "global": true
    },
    {
      "name": "ok",
      "type": "ACSubmit",
      "value": "OK",
      "uri": "/restart"
    },
    {
      "name": "cancel",
      "type": "ACSubmit",
      "value": "Cancel",
      "uri": "/_ac"
    }
  ]
}
)";

static const char AUX_RESTART[] PROGMEM = R"(
{
  "title": "Config IP",
  "uri": "/restart",
  "menu": false,
  "element": [
    {
      "name": "caption",
      "type": "ACText",
      "value": "Settings",
      "style": "font-family:Arial;font-weight:bold;text-align:center;margin-bottom:10px;color:steelblue",
      "posterior": "div"
    },
    {
      "name": "staip",
      "type": "ACText",
      "format": "IP: %s",
      "posterior": "br",
      "global": true
    },
    {
      "name": "gateway",
      "type": "ACText",
      "format": "Gateway: %s",
      "posterior": "br",
      "global": true
    },
    {
      "name": "netmask",
      "type": "ACText",
      "format": "Netmask: %s",
      "posterior": "br",
      "global": true
    },
    {
      "name": "dns1",
      "type": "ACText",
      "format": "DNS1: %s",
      "posterior": "br",
      "global": true
    },
    {
      "name": "dns2",
      "type": "ACText",
      "format": "DNS2: %s",
      "posterior": "br",
      "global": true
    },
    {
      "name": "result",
      "type": "ACText",
      "posterior": "par"
    }
  ]
}
)";

const static char addonJson[] PROGMEM = R"raw(
[
  {
    "title": "MQTT Settings",
    "uri": "/mqtt_setting",
    "menu": true,
    "element": [
      {
        "name": "header",
        "type": "ACText",
        "value": "<h2>MQTT broker settings</h2>",
        "style": "text-align:center;color:#2f4f4f;padding:10px;"
      },
      {
        "name": "caption",
        "type": "ACText",
        "value": "<h4>Set up MQTT Parameters</h4>",
        "style": "font-family:serif;color:#4682b4;"
      },
  
      {
        "name": "MqttID/Ip",
        "type": "ACInput",
        "value": "",
        "label": "MQTT Server / IP",
        "pattern": "^(([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\\-]*[a-zA-Z0-9])\\.)*([A-Za-z0-9]|[A-Za-z0-9][A-Za-z0-9\\-]*[A-Za-z0-9])$",
        "placeholder": "MQTT broker server"
        
      },
      {
        "name": "MqttPortNo",
        "type": "ACInput",
        "label": "Port No",
        "pattern": "^[0-9]{6}$",
        "placeholder": "1883"
      },
      {
        "name": "MqttUsername",
        "type": "ACInput",
        "label": "Username",
        "placeholder": "XXXXXX"
      },
      {
        "name": "MqttPassword",
        "type": "ACInput",
        "label": "Password",
        "placeholder": "XXXXXX"
      },
      {
        "name": "newline",
        "type": "ACElement",
        "value": "<hr>"
      },
      {
        "name": "save",
        "type": "ACSubmit",
        "value": "Save",
        "uri": "/mqtt_save"
      },
      {
        "name": "MainPage",
        "type": "ACSubmit",
        "value": "Main Page",
        "uri": "/_ac"
      }
    ]
  },
  {
    "title": "MQTT Setting",
    "uri": "/mqtt_save",
    "menu": false,
    "element": [
      {
        "name": "caption2",
        "type": "ACText",
        "value": "<h4>Parameters saved as:</h4>",
        "style": "text-align:center;color:#2f4f4f;padding:10px;"
      },
      {
        "name": "parameters",
        "type": "ACText"
      },
      {
        "name": "EspRestart",
        "type": "ACSubmit",
        "value": "ESP Restart",
        "uri": "/ESP_Restart"
       
      },
      {
        "name": "MainPage",
        "type": "ACSubmit",
        "value": "Main Page",
        "uri": "/_ac"
      }    
    ]
    
  },
  {
    "title": "Sensor Settings",
    "uri": "/sensor_setting",
    "menu": true,
    "element": [
      {
        "name": "header",
        "type": "ACText",
        "value": "<h2>ZigBee Sensor Settings</h2>",
        "style": "text-align:center;color:#2f4f4f;padding:10px;"
      },
      {
        "name": "caption",
        "type": "ACText",
        "value": "<h4>Setting up Sensor location, Sensor ID, Sensor Serial Number</h4>",
        "style": "font-family:serif;color:#4682b4;"
      },
      {
        "name": "SensorType",
        "type": "ACInput",
        "label": "Sensor Type",
        "pattern": "^(([a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9\\-]*[a-zA-Z0-9])\\.)*([A-Za-z0-9]|[A-Za-z0-9][A-Za-z0-9\\-]*[A-Za-z0-9])$",
        "placeholder": "BME280"
      },
      {
        "name": "SensorNumber",
        "type": "ACInput",
        "label": "Sensor Number",
        "placeholder": "BME280_1"
      },
      {
        "name": "DeviceLocation",
        "type": "ACInput",
        "label": "Location",
        "placeholder": "F1_R1"
      },
      {
        "name": "FloorNumber",
        "type": "ACInput",
        "label": "Floor Number",
        "pattern": "^[0-9]{6}$",
        "placeholder": "Range(0~255)"
      },
      {
        "name": "RoomNumber",
        "type": "ACInput",
        "label": "Room Number",
        "pattern": "^[0-9]{6}$",
        "placeholder": "Range(0~255)"
      },
      {
        "name": "SensorID",
        "type": "ACInput",
        "label": "Sensor ID",
        "pattern": "^[0-9]{6}$",
        "placeholder": "Range(0~255) Eg: 1"
      },
      {
        "name": "SensorSerialNumber",
        "type": "ACInput",
        "label": "Sensor Serial Number",
        "pattern": "^[0-9]{6}$",
        "placeholder": "Range(0~255)"
      },
      {
        "name": "newline",
        "type": "ACElement",
        "value": "<hr>"
      },
      {
        "name": "save",
        "type": "ACSubmit",
        "value": "SAVE",
        "uri": "/sensor_save"
      },
      {
        "name": "MainPage",
        "type": "ACSubmit",
        "value": "Main Page",
        "uri": "/_ac"
      }
    ]
  },
  {
    "title": "Sensor_Settings",
    "uri": "/sensor_save",
    "menu": false,
    "element": [
      {
        "name": "caption2",
        "type": "ACText",
        "value": "<h4>Save parameters</h4>",
        "style": "text-align:center;color:#2f4f4f;padding:10px;"
      },
      {
        "name": "parameters",
        "type": "ACText"
      },
      {
        "name": "EspRestart",
        "type": "ACSubmit",
        "value": "ESP Restart",
        "uri": "/ESP_Restart",
        "style": "font-family:serif;color:#4682b4;"
      },
      {
        "name": "MainPage",
        "type": "ACSubmit",
        "value": "Main Page",
        "uri": "/_ac"
      }
    ]
  } 
]
)raw";

const static char addonJsonEspRestart[] PROGMEM = R"raw(
[
  {
    "title": "ESP Restart",
    "uri": "/ESP_Restart",
    "menu": true,
    "element": []
  }
]
)raw";

String onsaveMqtt(AutoConnectAux& aux, PageArgument& args) {
  // Open a parameter file on the SPIFFS.
                
                // Get a '/sensor_setting' page
               auxPageMqtt  = Portal.aux("/mqtt_setting");

                  // Retrieve a server name from an AutoConnectText value.
                AutoConnectInput& Mqtt_ID = auxPageMqtt->getElement<AutoConnectInput>("MqttID/Ip");
                AutoConnectInput& MqttPort_No = auxPageMqtt->getElement<AutoConnectInput>("MqttPortNo");
                AutoConnectInput& Mqtt_Username = auxPageMqtt->getElement<AutoConnectInput>("MqttUsername");
                
                AutoConnectInput& Mqtt_Password = auxPageMqtt->getElement<AutoConnectInput>("MqttPassword");
                
                // Create a JSON document
                StaticJsonDocument<512> doc;
                doc["MQTT"]["MqttID"] = Mqtt_ID.value;
                doc["MQTT"]["MqttPortNo"] = MqttPort_No.value;
                doc["MQTT"]["MqttUsername"] = Mqtt_Username.value;
                doc["MQTT"]["MqttPassword"] = Mqtt_Password.value;
                
                
                // Serialize JSON to string
                String jsonString;
                serializeJsonPretty(doc, jsonString);
                Serial.println("MqttID Name");
                Serial.println(Mqtt_ID.value);

                SPIFFS.begin();
                if (!SPIFFS.begin(true)) {
                    Serial.println("An Error has occurred while mounting SPIFFS");
                    return "";
                }
                File paramFile = SPIFFS.open("/co-ordinator_config.json", FILE_WRITE);
                paramFile.println(jsonString);
                paramFile.close();
                Serial.println("Parameters saved to /co-ordinator_config.json.json");
                
                //Read parameters
                File param = SPIFFS.open("/co-ordinator_config.json", FILE_READ);
                if (!param) {
                    Serial.println("Failed to open file for reading");
                    return"";
                  }

                Serial.println("Reading parameters from /param:");
                while (param.available()) {
                String line = param.readStringUntil('\n');
                Serial.println(line);
              }

                param.close();
                
                SPIFFS.end();
                return jsonString;
                
}
String onsaveMqttSensor(AutoConnectAux& aux, PageArgument& args) {
  // Open a parameter file on the SPIFFS.
                String jsonString;
                auxPageSensor  = Portal.aux("/sensor_setting");

                  // Retrieve a server name from an AutoConnectText value.
                AutoConnectInput& Sensor_Type = auxPageSensor->getElement<AutoConnectInput>("SensorType");
                AutoConnectInput& Sensor_Number = auxPageSensor->getElement<AutoConnectInput>("SensorNumber");
                AutoConnectInput& Location = auxPageSensor->getElement<AutoConnectInput>("DeviceLocation");
                
                AutoConnectInput& Floor_No = auxPageSensor->getElement<AutoConnectInput>("FloorNumber");
                AutoConnectInput& Room_No = auxPageSensor->getElement<AutoConnectInput>("RoomNumber");
                AutoConnectInput& Sensor_Id = auxPageSensor->getElement<AutoConnectInput>("SensorID");
                AutoConnectInput& Serial_No = auxPageSensor->getElement<AutoConnectInput>("SensorSerialNumber");
                // Create a JSON document
                StaticJsonDocument<512> doc;
                doc["SENSOR"]["Sensor_Type"] = Sensor_Type.value;
                doc["SENSOR"]["Sensor_Number"] = Sensor_Number.value;
                doc["SENSOR"]["Location"] = Location.value;

                doc["Map"]["Floor_No"] = Floor_No.value;
                doc["Map"]["Room_No"] = Room_No.value;
                doc["Map"]["Sensor_Id"] = Sensor_Id.value;
                doc["Map"]["Serial_No"] = Serial_No.value;

                // Serialize JSON to string
                
                serializeJsonPretty(doc, jsonString);
                Serial.println(jsonString);
                Serial.println("sensor Model");
                Serial.println(Sensor_Type.value);

                SPIFFS.begin();
                if (!SPIFFS.begin(true)) {
                    Serial.println("An Error has occurred while mounting SPIFFS");
                    return "";
                }
                File paramFile = SPIFFS.open("/sensor_config_file.json", FILE_WRITE);
                paramFile.println(jsonString);
                paramFile.close();
                Serial.println("Parameters saved to /sensor_config_file.json");
                
                File param = SPIFFS.open("/sensor_config_file.json", FILE_READ);
                if (!param) {
                  Serial.println("Failed to open file for reading");
                  return "";
                }

                Serial.println("Reading parameters from /param:");
                while (param.available()) {
                  String line = param.readStringUntil('\n');
                  Serial.println(line);
                }

                param.close();
                SPIFFS.end();
                return jsonString;
                
}
String onEspRestart(AutoConnectAux& aux, PageArgument& args) {

                Serial.println("Restarted Esp Successfully, Prameters succefully updated into Esp Flash Memory");
                String Results;
                Results = "Restarted Esp Successfully, Prameters succefully updated into Esp Flash Memory";
                ESP.restart();
                return Results;
  
}


void MQTT::begin() {                                                                                      // Begin WiFi connection for MQTT
    
   // espClient.setCACert(_ca_cert);
    mqtt_client.setServer(_mqtt_broker, _mqtt_port);
    //mqtt_client.setKeepAlive(60);                                                                      // Set keep-alive interval (e.g., 60 seconds) 
    mqtt_client.setCallback(MQTT::Subscribe);                                                                // Set callback to static function
    MQTT::reconnect_client();
    mqtt_client.subscribe(SubscribeTopic);
}
void MQTT::reconnect_client() {                                                                            // Reconnect MQTT client      // PROBLEM, MQTT PORT NUMBER , BROKER ID, USERNAME AND PASSWORD, IT BECOME WRONG VALUES OR VANISHED
    //delay(1000);
    int retryCount = 0;
    const int maxRetries = 120;                                                                              // Set a limit on the number of retries
    
    espClient.setCACert(_ca_cert);
    while (!mqtt_client.connected()&& retryCount < maxRetries) {
        //String client_id = "esp32-BME280" + String(WiFi.macAddress());
        String client_id = "esp32-BME280-Sensor-1";
        Serial.printf("Connecting to MQTT Broker as %s.....\n", client_id.c_str());
        if (mqtt_client.connect(client_id.c_str(), _mqtt_username, _mqtt_password)) {
            Serial.println("Connected to MQTT broker");
            mqtt_client.subscribe(SubscribeTopic);
            pinMode(5,OUTPUT);                     // RED LED is warning ststus to check ESP whether connected to MQTT broker or not
            digitalWrite(5,LOW);
            
            //WiFi.softAPdisconnect(true);
            //WiFi.enableAP(false);
            //config.autoRise = true; 
            //config.retainPortal = false;                   // retainPortal = false; will STOP SoftAP ,option will not shut down SoftAP and the internal DNS server even though AutoConnect::begin has aborted due to a timeout occurrence. Even after the captive portal times out, you can always try to connect to the AP while keeping the Sketch running offline.
            //Portal.config(config);                        // i add portal function, if autoconnect STA gateway is same as Router gateway, it will cause error , because KIOT gateway is 192.168.1.1, but Wi-copy gateway is 10.10.1.1, because different gateways,of wifi, it fail to connect MQTT, so it countinue like loop, STA Web GUI  can't accessed due to gateway conflicts, so i activate AP mode also , so user can change Wifi static ip config in Ap mode
            
           
        } else {
          
           // WiFi.softAPdisconnect(false);
            //WiFi.enableAP(true);
            //config.autoRise = true; 
            //config.retainPortal = true;                   // config.retainPortal = true; START SoftAPRetains the portal function after timed-out,option will not shut down SoftAP and the internal DNS server even though AutoConnect::begin has aborted due to a timeout occurrence. Even after the captive portal times out, you can always try to connect to the AP while keeping the Sketch running offline.
            //Portal.config(config);                        // i add portal function, if autoconnect STA gateway is same as Router gateway, it will cause error , because KIOT gateway is 192.168.1.1, but Wi-copy gateway is 10.10.1.1, because different gateways,of wifi, it fail to connect MQTT, so it countinue like loop, STA Web GUI  can't accessed due to gateway conflicts, so i activate AP mode also , so user can change Wifi static ip config in Ap mode
            
            
            Serial.print("Failed to connect to MQTT broker, rc=");
            Serial.println(mqtt_client.state());
            Portal.handleClient();
            delay(2000);
            pinMode(5,OUTPUT);                              // RED LED is warning ststus to check ESP whether connected to MQTT broker or not
            digitalWrite(5,HIGH);
            retryCount++;
        }
    }
    if (retryCount >= maxRetries) {
        Serial.println("Maximum retries reached, restarting...");
        ESP.restart();
    }
}
void MQTT::Publish_Sensor_data(uint8_t* JsonData, size_t length, uint8_t floorNo, uint8_t roomNo, uint8_t sensorID, uint8_t sensorNo){
  DynamicJsonDocument doc(256);
  char serializedJson[256];
  char dynamicTopic[50];
  DeserializationError error = deserializeJson(doc, JsonData);                                                              // Deserialize JSON data
    if (error) {
        Serial.print("JSON parsing failed: ");
        Serial.println(error.c_str());
        return;
    } 
    if(mqtt_client.connected()) {                                                                                                    
        snprintf(dynamicTopic, sizeof(dynamicTopic), "ESP/HOME/%02X/%02X/%02X/%02X", floorNo, roomNo, sensorID, sensorNo);  // create dynamic topic based on env and espid       
        size_t serializedSize = serializeJson(doc, serializedJson, sizeof(serializedJson));                                      // Serialize the JSON document into a string
        if (serializedSize == 0) {
            Serial.println("Failed to serialize JSON data.");
            return;
        }
        mqtt_client.publish(dynamicTopic, serializedJson);                                                                            // Publish the serialized JSON data

    } else {
        mqtt_client.disconnect();
        Serial.println("Failed to connect to MQTT broker.");
    }
    doc.clear();
}
void MQTT::Publish_Feedback_data(uint8_t feedback, uint8_t floorNo, uint8_t roomNo, uint8_t sensorID, uint8_t sensorNo) {
    
    DynamicJsonDocument doc(256);
    char dynamicFeedbackTopic[50];
    char serializedJson[256];
    
    if(mqtt_client.connected()) {
        
        // Create a dynamic topic
        snprintf(dynamicFeedbackTopic, sizeof(dynamicFeedbackTopic), "FeedbackS/HOME/%02X/%02X/%02X/%02X", floorNo, roomNo, sensorID, sensorNo);

        // Add feedback to the JSON object
        doc["feedback"] = feedback;

        // Serialize the JSON document
        size_t serializedSize = serializeJson(doc, serializedJson, sizeof(serializedJson));

        if (serializedSize == 0) {
            Serial.println("Failed to serialize JSON data.");
            return;
        }

        // Print and publish the JSON
        Serial.println();
        Serial.println(serializedJson);
        mqtt_client.publish(dynamicFeedbackTopic, serializedJson);
    } else {
        Serial.println("MQTT client is not connected.");
    }
}
void MQTT::setCallback(void (*callback)(char*, byte*, unsigned int)) {
    mqtt_client.setCallback(callback);
}
void MQTT::Subscribe(char* SubscribeTopic, byte* payload, unsigned int length) {
    Serial.println();
    Serial.print("Message arrived in topic: ");
    Serial.println(SubscribeTopic);
    Serial.println("Message:");
    for (int i = 0; i < length; i++) {
        Serial.print((char) payload[i]);
    }
    //Serial.println();
    
  /*
    // Store the payload data
    if (length < sizeof(payloadData)) {
        memcpy(payloadData, payload, length);
        payloadData[length] = '\0';  // Null-terminate the string
    } else {
        Serial.println("Payload too large to store in buffer");
        return;
    }   */
}
void MQTT::mqtt_client_loop(){
  mqtt_client.loop();
}


void Autoconnect::Autoconnect_begin(){
    pinMode(7,OUTPUT);                                      // BLUE_LED  is Indication status to check ESP is AP mode or station mode
    digitalWrite(7,HIGH);
    Serial.begin(115200);
    
    IPCONFIG ipconfig;
    loadConfig(&ipconfig);
    
    config.title = "ESP SETUP";
    config.apid = "ESP-BME280";
    config.psk  = "BME280ESP";
    //config.immediateStart = true;                         // portal on demand, skip first STA connect and raise an AP immediately
    //config.portalTimeout = 60000;                         // Sets timeout value for the captive portal, i think , after 1 minute it continue next part in sketch by ignoring portal.begin, but its still in AP mode you access web ui to connected wifi,but i won't reccommend , because mqtt will not connect and cause restart frequently
    //config.autoRise = true;                               // true - It's the default, no setting is needed explicitly. if it is flase - Suppresses the launch of the captive portal from AutoConnect::begin.
    //config.retainPortal = true;                             // Retains the portal function after timed-out,option will not shut down SoftAP and the internal DNS server even though AutoConnect::begin has aborted due to a timeout occurrence. Even after the captive portal times out, you can always try to connect to the AP while keeping the Sketch running offline.
    //config.apip = IPAddress(192,168,10,101);              // Sets SoftAP IP address
    //config.gateway = IPAddress(192,168,10,1);             // Sets WLAN router IP address
    //config.netmask = IPAddress(255,255,255,0);            // Sets WLAN scope
    //config.autoReconnect = true;                         // Enable auto-reconnect

    config.boundaryOffset = sizeof(IPCONFIG);               // Reserve  bytes for the user data in EEPROM.
    config.homeUri = "/dir";                                // Sets home path of Sketch application
    //config. preserveAPMode = true;                         
    IPAddress STAip = IPAddress(ipconfig.ipconfig.ip);
    IPAddress STAGateway = IPAddress(ipconfig.ipconfig.gateway);
    IPAddress STANetmask = IPAddress(ipconfig.ipconfig.netmask);
    IPAddress STAdns1 = IPAddress(ipconfig.ipconfig.dns1);
    IPAddress STAdns2 = IPAddress(ipconfig.ipconfig.dns2);

    Serial.println();
    Serial.println("EEPROM WiFi  IP: " + STAip.toString());
    Serial.println("EEPROM gateway: " + STAGateway.toString());
    Serial.println("EEPROM netmask: " + STANetmask.toString());
    Serial.println("EEPROM primary DNS: " + STAdns1.toString());
    Serial.println("EEPROM secondary DNS: " + STAdns2.toString());
    
    config.ota = AC_OTA_BUILTIN;                          // ENABLE TO OTA Mode 
    config.otaExtraCaption = fw_ver;                      // To display in the add an extra caption to the OTA update screen
    
    Portal.config(config);                                // Use Portal, not portal
    Portal.append("/_ac", "Statistics");
    
    auxIPConfig.load(AUX_CONFIGIP);
    auxIPConfig.on([this](AutoConnectAux& aux, PageArgument& args) { return this->getConfig(aux, args); });
    auxRestart.load(AUX_RESTART);
    auxRestart.on([this](AutoConnectAux& aux, PageArgument& args) { return this->setConfig(aux, args); });
    
    auxPageMqtt  = Portal.aux("/mqtt_setting");
    auxPageSensor = Portal.aux("/sensor_setting");
    SaveMqtt = Portal.aux("/mqtt_save");
    SaveSensor = Portal.aux("/sensor_save");
    EspRestart = Portal.aux("/ESP_Restart");

    Portal.load(addonJson);
    // Portal.load(addonJsonSensor);                      // Uncomment if needed
    Portal.load(addonJsonEspRestart);

    Portal.join({ auxPageMqtt, SaveMqtt });
    Portal.join({ auxPageSensor, SaveSensor });
    Portal.join({ auxIPConfig, auxRestart });
    
    Portal.on("/mqtt_save", onsaveMqtt);
    Portal.on("/sensor_save", onsaveMqttSensor);
    Portal.on("/ESP_Restart", onEspRestart);
    
    IPAddress connectedGateway;
    IPAddress connectedNetmask;
    IPAddress primaryDNS;
    IPAddress secondaryDNS;
    
    if (Portal.begin()) {

        connectedGateway = WiFi.gatewayIP();
        connectedNetmask = WiFi.subnetMask();
        primaryDNS = WiFi.dnsIP(0);                       // Primary DNS
        secondaryDNS = WiFi.dnsIP(1);                     // Secondary DNS
        
        Serial.println();
        Serial.println("WiFi connected: " + WiFi.localIP().toString());
        Serial.println("Connected Wi-Fi gateway: " + connectedGateway.toString());
        Serial.println("Connected Wi-Fi netmask: " + connectedNetmask.toString());
        Serial.println("Connected Wi-Fi primary DNS: " + primaryDNS.toString());
        Serial.println("Connected Wi-Fi secondary DNS: " + secondaryDNS.toString());
        if (connectedGateway != STAGateway && STAGateway != IPAddress(0, 0, 0, 0) ) {
                Serial.println();
                Serial.println("Both Wi-Fi Router Gateway and ESP (EEPROM) Gateway is NOT same");
                if (!EEPROM.begin(EEPROM_SIZE)) {
                    Serial.println("Failed to initialise EEPROM");
                    ESP.restart();
                    //return;
                }
                for (int i = 0; i < EEPROM_SIZE; i++) {                        // Write 0xFF to each byte in the EEPROM
                    EEPROM.write(i, 0xFF);                                     // You could use 0x00 if you prefer to set all bytes to zero
                }
                EEPROM.commit();                                                // Commit the changes to save them to flash memory
                Serial.println("EEPROM erased.");
                ESP.restart();
                
        }else if (STAGateway == IPAddress(0, 0, 0, 0)){
                 Serial.println();
                 Serial.println("ESP (EEPROM) Gateway is NULL, so ESP gateway change to wifi router gateway");
                 Serial.println();
                 digitalWrite(7,LOW);
                if (MDNS.begin("bme280")) { // Set the hostname to "esp32.local"
                    MDNS.addService("http", "tcp", 80);
                }
        }
                            
    }
    
    Serial.println("Connected Wi-Fi gateway1: " + connectedGateway.toString());
    
    if(connectedGateway == STAGateway){
          Serial.println();
          Serial.println("Both Wi-Fi Router Gateway and ESP (EEPROM) Gateway is same");
          //Portal.end();
          
          config.staip = IPAddress(ipconfig.ipconfig.ip);
          config.staGateway = IPAddress(ipconfig.ipconfig.gateway);
          config.staNetmask = IPAddress(ipconfig.ipconfig.netmask);
          config.dns1 = IPAddress(ipconfig.ipconfig.dns1);
          config.dns2 = IPAddress(ipconfig.ipconfig.dns2);
          config.preserveIP = true;
          Portal.config(config);
          Portal.begin();
          
          Serial.println();
          Serial.println("Static ESP WiFi IP: " + config.staip.toString());
          Serial.println("Static EEPROM gateway: " + config.staGateway.toString());
          Serial.println("Static EEPROM netmask: " + config.staNetmask.toString());
          Serial.println("Static EEPROM primary DNS: " + config.dns1.toString());
          Serial.println("Static EEPROM secondary DNS: " + config.dns2.toString());
          delay(2000);
          
          IPAddress connectedGateway1 = WiFi.gatewayIP();
          IPAddress connectedNetmask1 = WiFi.subnetMask();
          IPAddress primaryDNS1 = WiFi.dnsIP(0); // Primary DNS
          IPAddress secondaryDNS1 = WiFi.dnsIP(1); // Secondary DNS
        
          Serial.println();
          Serial.println("WiFi connected1: " + WiFi.localIP().toString());
          Serial.println("Connected Wi-Fi gateway1: " + connectedGateway1.toString());
          Serial.println("Connected Wi-Fi netmask1: " + connectedNetmask1.toString());
          Serial.println("Connected Wi-Fi primary DNS1: " + primaryDNS1.toString());
          Serial.println("Connected Wi-Fi secondary DNS1: " + secondaryDNS1.toString());
          Serial.println();
          digitalWrite(7,LOW);
          if (MDNS.begin("bme280")) { // Set the hostname to "esp32.local"
              MDNS.addService("http", "tcp", 80);
          }
    }

    
  
}
void Autoconnect::Autoconnect_handleClient(){
  Portal.handleClient();
 
}
void Autoconnect::Autoconnect_AP_STA(){
  
      //WiFi.softAPdisconnect(true);
      //WiFi.enableAP(false);
      config.retainPortal = true;                   // Retains the portal function after timed-out,option will not shut down SoftAP and the internal DNS server even though AutoConnect::begin has aborted due to a timeout occurrence. Even after the captive portal times out, you can always try to connect to the AP while keeping the Sketch running offline.
      Portal.config(config);
}
void Autoconnect::loadConfig(IPCONFIG* ipconfig) {
    EEPROM.begin(EEPROM_SIZE);
    int dp = 0;
    for (uint8_t i = 0; i < 5; i++) {
        for (uint8_t c = 0; c < sizeof(uint32_t); c++)
            ipconfig->ipraw[c + i * sizeof(uint32_t)] = EEPROM.read(dp++);
    }
    EEPROM.end();

    if (ipconfig->ipconfig.ip == 0xffffffffL)
        ipconfig->ipconfig.ip = 0U;
    if (ipconfig->ipconfig.gateway == 0xffffffffL)
        ipconfig->ipconfig.gateway = 0U;
    if (ipconfig->ipconfig.netmask == 0xffffffffL)
        ipconfig->ipconfig.netmask = 0U;
    if (ipconfig->ipconfig.dns1 == 0xffffffffL)
        ipconfig->ipconfig.dns1 = 0U;
    if (ipconfig->ipconfig.dns2 == 0xffffffffL)
        ipconfig->ipconfig.dns2 = 0U;
/*
    Serial.println("IP configuration loaded");
    Serial.printf("Sta IP :0x%08lx\n", (long unsigned int)ipconfig->ipconfig.ip);
    Serial.printf("Gateway:0x%08lx\n", (long unsigned int)ipconfig->ipconfig.gateway);
    Serial.printf("Netmask:0x%08lx\n", (long unsigned int)ipconfig->ipconfig.netmask);
    Serial.printf("DNS1   :0x%08lx\n", (long unsigned int)ipconfig->ipconfig.dns1);
    Serial.printf("DNS2   :0x%08lx\n", (long unsigned int)ipconfig->ipconfig.dns2);*/
}
void Autoconnect::saveConfig(const IPCONFIG* ipconfig) {
    EEPROM.begin(EEPROM_SIZE);

    int dp = 0;
    for (uint8_t i = 0; i < 5; i++) {
        for (uint8_t d = 0; d < sizeof(uint32_t); d++)
            EEPROM.write(dp++, ipconfig->ipraw[d + i * sizeof(uint32_t)]);
    }
    EEPROM.end();
    delay(100);
}
void Autoconnect::getIPAddress(String ipString, uint32_t* ip) {
    IPAddress ipAddress;

    if (ipString.length())
        ipAddress.fromString(ipString);
    *ip = (uint32_t)ipAddress;
}
String Autoconnect::getConfig(AutoConnectAux& aux, PageArgument& args) {
    IPCONFIG ipconfig;
    loadConfig(&ipconfig);

    String macAddress;
    uint8_t mac[6];
    WiFi.macAddress(mac);
    for (uint8_t i = 0; i < 6; i++) {
        char buf[3];
        sprintf(buf, "%02X", mac[i]);
        macAddress += buf;
        if (i < 5)
            macAddress += ':';
    }
    aux["mac"].value = macAddress;

    IPAddress staip = IPAddress(ipconfig.ipconfig.ip);
    IPAddress gateway = IPAddress(ipconfig.ipconfig.gateway);
    IPAddress netmask = IPAddress(ipconfig.ipconfig.netmask);
    IPAddress dns1 = IPAddress(ipconfig.ipconfig.dns1);
    IPAddress dns2 = IPAddress(ipconfig.ipconfig.dns2);

    aux["staip"].value = staip.toString();
    aux["gateway"].value = gateway.toString();
    aux["netmask"].value = netmask.toString();
    aux["dns1"].value = dns1.toString();
    aux["dns2"].value = dns2.toString();

    return String();
}
String Autoconnect::setConfig(AutoConnectAux& aux, PageArgument& args) {
    IPCONFIG ipconfig;

    getIPAddress(aux["staip"].value, &ipconfig.ipconfig.ip);
    getIPAddress(aux["gateway"].value, &ipconfig.ipconfig.gateway);
    getIPAddress(aux["netmask"].value, &ipconfig.ipconfig.netmask);
    getIPAddress(aux["dns1"].value, &ipconfig.ipconfig.dns1);
    getIPAddress(aux["dns2"].value, &ipconfig.ipconfig.dns2);

    if (auxIPConfig.isValid()) {
      saveConfig(&ipconfig);
      aux["result"].value = "Reset by AutoConnect menu will restart with the above.";
    } else {
      aux["result"].value = "Invalid IP address specified.";
    }
    
    return String();
} 
