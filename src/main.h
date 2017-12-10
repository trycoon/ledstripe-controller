#ifndef main_h
#define main_h

#include <string>
#include <Arduino.h>
#include <AsyncMqttClient.h>

const uint8_t WARDROBE_LED_PIN = D6;
const char* APP_NAME = "entrance-wardrobe";

const char* SSID = "<enter your SSID here>";
const char* WIFI_PASSWORD = "<enter wifi password>";

const char* OTA_PASSWORD = "<enter OTA password>";

const char* MQTT_SERVER = "<enter MQTT broker IP>";
const char* MQTT_TOPIC = "home/entrance-wardrobe";
const uint16_t MQTT_PORT = 1883;

uint8_t lightLevel;
boolean spiffs_available;

void onMqttConnect(bool sessionPresent);
void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);
void onMqttPublish(uint16_t packetId);
void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
void publish_message(String message, std::string subtopic = "");
void setLightLevel(uint8_t level);
void getLightLevel();
void setup_WiFi();
void setup_OTA();
void setup_MQTT();

#endif
