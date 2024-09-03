#ifndef WEB_UI_H               
#define WEB_UI_H               

#include <Arduino.h>
#include <WiFi.h>
#include <AutoConnect.h>

// Define EEPROM_SIZE according to your needs
#define EEPROM_SIZE 64

typedef union {
    struct {
        uint32_t ip;
        uint32_t gateway;
        uint32_t netmask;
        uint32_t dns1;
        uint32_t dns2;
    } ipconfig;
    uint8_t ipraw[sizeof(uint32_t) * 5];
} IPCONFIG;


class MQTT{
  public :
      MQTT() {} 
      MQTT(const char *mqtt_broker, int mqtt_port, const char *mqtt_username, const char *mqtt_password, const char *ca_cert);
     
      
      void begin();
      void reconnect_client();
      void Publish_Sensor_data(uint8_t* JsonData, size_t length, uint8_t Floor_No, uint8_t Room_No, uint8_t Sensor_ID, uint8_t Sensor_No);
      void Publish_Feedback_data(uint8_t Feedback,  uint8_t floorNo, uint8_t roomNo, uint8_t sensorID, uint8_t sensorNo);
      static void Subscribe(char* SubscribeTopic, byte* payload, unsigned int length); // Declare as static
      void setCallback(void (*callback)(char*, byte*, unsigned int));
      void mqtt_client_loop();


      
     
  private:
       const char *_mqtt_broker;
       int _mqtt_port;
       const char *_mqtt_username;
       const char *_mqtt_password;
       const char *_ca_cert;
       
       
       static  char payloadData[256];  // Buffer to store payload data, so it can store 255 characters and one byte for null character
       
       
     
};



class Autoconnect{
  public :
      void Autoconnect_begin();
      void Autoconnect_handleClient();
      void Autoconnect_AP_STA();
      void loadConfig(IPCONFIG* ipconfig);
      void saveConfig(const IPCONFIG* ipconfig);
      void getIPAddress(String ipString, uint32_t* ip);
      String getConfig(AutoConnectAux& aux, PageArgument& args);
      String setConfig(AutoConnectAux& aux, PageArgument& args);

  private:
      
      
};



#endif
