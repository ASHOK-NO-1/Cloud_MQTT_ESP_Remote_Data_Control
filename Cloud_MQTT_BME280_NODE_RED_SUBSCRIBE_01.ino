// Libraries
#include "WEB_UI.h"
#include "ESPFileServer.h"
#include <SPIFFS.h>         
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#define I2C_SDA 15
#define I2C_SCL 4

#define Boot_Press_Time 5000  // 5000 milliseconds = 5 seconds
#define Boot_Button    0              
#define GREEN_LED     6
#define RED_LED       5 
#define BLUE_LED      7

TwoWire I2CBME = TwoWire(0);
Adafruit_BME280 bme;

int RED_LED_STATE = HIGH;                                                                   // the current state of button
uint8_t GREEN_LED_STATE = LOW;
int BLUE_LED_STATE   = LOW;


MQTT mqtt;
Autoconnect autoconnect;
ESPFileServer flash_server;
DynamicJsonDocument doc(238);
bool jsonDataRead = false; // Static flag to track if JSON data has been read

String  sensor_type;
String  sensor_number;
String  location;
uint8_t Floor_No;
uint8_t Room_No; 
uint8_t Sensor_ID; 
uint8_t Sensor_No;
float temp;
float hum;



// Function prototypes
void Read_EspFlash_Sensor_JsonFile();
void setup();
void handlePayload(byte* payload, unsigned int length);
void WiFi_Client_Connection(void *pvParameters);
void printValues(void *pvParameters);
void ZIGBEE_Json_data_write(float temp, float hum);
void loop();


void Read_EspFlash_Sensor_JsonFile(){
    if (!SPIFFS.begin(true)) {
        Serial.println("An error occurred while mounting SPIFFS");
        while(true) {
            delay(1000);                                                                      // Delay to keep the error message visible
        }
    }
    
    // Load JSON data from file
    File configFile = SPIFFS.open("/sensor_config_file.json", "r");                           // r stands for read mode
    if (!configFile) {
        Serial.println("Failed to open config file");
        while(true) {
            delay(1000);                                                                        // Delay to keep the error message visible
        }
    }
    
    // Parse JSON data
    size_t size = configFile.size();
    std::unique_ptr<char[]> buffer(new char[size]);
    configFile.readBytes(buffer.get(), size);
    configFile.close();
    
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, buffer.get());
    if (error) {
        Serial.println("Failed to parse config file");
        Serial.println(error.c_str());
        
        while(true) {
            delay(1000);                                                                         // Delay to keep the error message visible
        }
    }
    // Accessing values
    sensor_type = doc["SENSOR"]["Sensor_Type"].as<String>();
    sensor_number = doc["SENSOR"]["Sensor_Number"].as<String>();
    location = doc["SENSOR"]["Location"].as<String>();
    Floor_No = static_cast<uint8_t>(doc["Map"]["Floor_No"].as<String>().toInt());
    Room_No = static_cast<uint8_t>(doc["Map"]["Room_No"].as<String>().toInt());
    Sensor_ID = static_cast<uint8_t>(doc["Map"]["Sensor_Id"].as<String>().toInt());
    Sensor_No = static_cast<uint8_t>(doc["Map"]["Serial_No"].as<String>().toInt());

    
  
}

const char* readFile(const char* path) {                    // Define file-related functions   Read MQTT Certificate from SPIFFS
    File file = SPIFFS.open(path, "r");
    if (!file) {
        Serial.println("Failed to open file for reading");
        return nullptr;
    }

    size_t size = file.size();
    if (size == 0) {
        Serial.println("File is empty");
        return nullptr;
    }

    // Allocate a buffer to store the file contents
    std::unique_ptr<char[]> buffer(new char[size + 1]);
    file.readBytes(buffer.get(), size);
    buffer[size] = '\0'; // Null-terminate the buffer
    file.close();

    return buffer.release(); // Return the buffer content
}

void setup() {
    Serial.begin(115200);

    pinMode(GREEN_LED,OUTPUT);
    digitalWrite(GREEN_LED,LOW);
    
    flash_server.begin();                                                        // COMMENT server.begin in ESPFileServer.cpp library, because, if user start server.begin before autoconnect library( portal.begin), both libraries use webserver library, so both will conflict
    autoconnect.Autoconnect_begin();                                            // because if user want see spiffs files without connecting wifi, when they use autoconnect library, 
    
    
    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("An error occurred while mounting SPIFFS");
        
    }
    
    // Load JSON data from file
    File configFile = SPIFFS.open("/co-ordinator_config.json", "r"); // r stands for read mode
    if (!configFile) {
        Serial.println("Failed to open config file");
        
    }
    
    // Parse JSON data
    size_t size = configFile.size();
    std::unique_ptr<char[]> buffer(new char[size]);
    configFile.readBytes(buffer.get(), size);
    configFile.close();
    
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, buffer.get());
    if (error) {
        Serial.println("Failed to parse config file");
        
    }

    const char *mqtt_ip = doc["MQTT"]["MqttID"];
    const int mqtt_port = doc["MQTT"]["MqttPortNo"];
    const char *mqtt_Username = doc["MQTT"]["MqttUsername"];
    const char *mqtt_Password = doc["MQTT"]["MqttPassword"];

    // Read CA certificate from SPIFFS
    const char* mqtt_ca_cert = readFile("/emqxsl-ca.crt");
    if (mqtt_ca_cert != nullptr) {
        Serial.println("CA Certificate read successfully");
    } else {
        Serial.println("Failed to read CA Certificate");
        
    }
    
   
    
    // Initialize and return MQTT object with extracted parameters
    mqtt = MQTT(mqtt_ip, mqtt_port, mqtt_Username, mqtt_Password, mqtt_ca_cert);
    mqtt.begin();
    mqtt.setCallback(handlePayload);
    
    //flash_server.begin();                                                    // uncomment server.begin in ESPFileServer.cpp library,when user want see spiffs files only connected wifi, when they use autoconnect library or not
  
 /*   if (!SPIFFS.begin(true)) {
        Serial.println("An error occurred while mounting SPIFFS");
        
    }*/
    Serial.println(F("BME280 test"));
    I2CBME.begin(I2C_SDA, I2C_SCL, 100000);

    bool status;
    status = bme.begin(0x77, &I2CBME);  
    if (!status) {
      Serial.println("Could not find a valid BME280 sensor, check wiring!");
      while (1);
    }
    Serial.println("-- BME280 sensor found --");
    
    
    
    xTaskCreatePinnedToCore(SensorValues, "Task1", 10000, NULL, 7, NULL, 0);
    xTaskCreatePinnedToCore(WiFi_Client_Connection, "Task2", 30000, NULL, 9, NULL, 0);
    xTaskCreatePinnedToCore(Autoconnect_AP_STA, "Task1", 10000, NULL, 8, NULL, 0);

}

void handlePayload(char* topic, byte* payload, unsigned int length) {
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("]: ");
    for (unsigned int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
    }
    Serial.println();

    if (length > 0) {
        if (payload[0] == '1') {
            
            GREEN_LED_STATE = HIGH;
            digitalWrite(GREEN_LED, HIGH);  
        } else if (payload[0] == '0') {
            
            GREEN_LED_STATE = LOW;
            digitalWrite(GREEN_LED, LOW);  
        }
        mqtt.Publish_Feedback_data(GREEN_LED_STATE, Floor_No, Room_No, Sensor_ID, Sensor_No);
    }
}

void WiFi_Client_Connection(void *pvParameters) {
    (void) pvParameters; 
    for (;;) {
      
        vTaskDelay(200);
        mqtt.reconnect_client();   
        vTaskDelay(200);
    }
}

void Autoconnect_AP_STA(void *pvParameters) {
    (void) pvParameters;   // To avoid unused variable warning
    pinMode(Boot_Button, INPUT_PULLUP);
    pinMode(BLUE_LED, OUTPUT);

    uint32_t pressStartTime = 0;
    bool isPressed = false;

    for (;;) {
        
        if (digitalRead(Boot_Button) == LOW) {                                   // Check if the boot button is pressed (active low)
            if (!isPressed) {
                isPressed = true;
                pressStartTime = millis();
            } else {
                
                if (millis() - pressStartTime > Boot_Press_Time) {                // If button is held down for more than 5 seconds
                    autoconnect.Autoconnect_AP_STA();
                    digitalWrite(BLUE_LED, HIGH);                         
                                                         
                }
            }
        } else {
            
            isPressed = false;                                                           // Reset the state if the button is released
            
        }

        vTaskDelay(500);
    }

}
void SensorValues(void *pvParameters) {
  (void) pvParameters; 
  for(;;){
        temp = bme.readTemperature();
        hum  = bme.readHumidity();
        Serial.print("Temperature: ");
        Serial.println(temp);
        Serial.print("Humidity: ");
        Serial.println(hum);
        ZIGBEE_Json_data_write(temp, hum);
        delay(60000);                     
        
}
}

void ZIGBEE_Json_data_write(float temp, float hum) {
  
            if (!jsonDataRead) {
              Read_EspFlash_Sensor_JsonFile();
              jsonDataRead = true;
          }
            doc.clear(); 
            doc["Sensor_Type"] = sensor_type;
            doc["Sensor_Number"] = sensor_number;
            doc["Location"] = location;
                                 
            // Format the float values with two decimal places
            String tempStr = String(temp, 2);
            String humStr = String(hum, 2);
            JsonObject data = doc.createNestedObject("data");
            data["temperature"] = tempStr; 
            data["humidity"] = humStr;             
            char output[238];
            serializeJson(doc, output);
            Serial.println(output);
            
            // Cast the output array to uint8_t* when calling ZIGBEE_write
            mqtt.Publish_Sensor_data(reinterpret_cast<uint8_t*>(output), strlen(output), Floor_No, Room_No, Sensor_ID, Sensor_No);
            delay(1000);
            mqtt.Publish_Feedback_data(GREEN_LED_STATE, Floor_No, Room_No, Sensor_ID, Sensor_No);
 }


void loop() {
    autoconnect.Autoconnect_handleClient();
    mqtt.mqtt_client_loop();
    flash_server.Spiffs_handleClient();
    delay(10);
}
