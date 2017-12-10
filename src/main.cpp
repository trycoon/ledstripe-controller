#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <AsyncMqttClient.h>
#include <string>
#include <main.h>
#include "FS.h"

/*
 * Simple NodeMCU sketch that controlls the LED-stripe in the entrance wardrobe
 */

AsyncMqttClient mqttClient;

void setup() {
  Serial.begin(115200);
  Serial.println("Entrance wardrobe manager v1.0");

  pinMode(WARDROBE_LED_PIN, OUTPUT);
  lightLevel = 0;

  spiffs_available = SPIFFS.begin();
  Serial.print("SPIFFS opened: ");
  Serial.println(spiffs_available);

  if (spiffs_available) {
    File f = SPIFFS.open("/lightLevel", "r");
    lightLevel = f.parseInt();
    Serial.print("read lightlevel from SPIFFS: ");
    Serial.println(lightLevel);
    f.close();
  } else {
    if (SPIFFS.format()) {
      File fw = SPIFFS.open("/lightLevel", "w");
      fw.println(lightLevel);
      fw.close();
      Serial.println("SPIFFS formated.");
    }
  }

  setup_WiFi();
  setup_OTA();
  setup_MQTT();

  setLightLevel(lightLevel);
}

void setLightLevel(uint8_t level) {
  lightLevel =  constrain(level, 0, 100);
  // Save lightlevel to SPIFFS (EEPROM), to remember value between powercycles.
  if (spiffs_available) {
    File f = SPIFFS.open("/lightLevel", "w");
    f.println(lightLevel);
    f.close();
    Serial.print("saved lightlevel to SPIFFS: ");
    Serial.println(lightLevel);
  }

  analogWrite(WARDROBE_LED_PIN, map(lightLevel, 0, 100, 0, PWMRANGE));
  getLightLevel();

  Serial.print("set lightlevel to: ");
  Serial.println(lightLevel);
}

void getLightLevel() {
  publish_message(String(lightLevel), "lightlevel");
}

void onWiFiDisconnected(const WiFiEventStationModeDisconnected& event) {
  Serial.println("Lost WiFi connection!");
}

void setup_WiFi() {

  uint8_t MAC_array[6];
  char MAC_char[18];

  WiFi.macAddress(MAC_array);
  for (int i = 0; i < sizeof MAC_array; ++i) {
    snprintf(MAC_char, sizeof MAC_char, "%s%02x:", MAC_char, MAC_array[i]);
  }

  Serial.println(MAC_char);

  WiFi.onStationModeDisconnected(&onWiFiDisconnected);
  WiFi.hostname(APP_NAME);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(SSID, WIFI_PASSWORD);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("WiFi connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
}

void setup_OTA() {
  // Port defaults to 8266
  ArduinoOTA.setPort(8266);

  ArduinoOTA.setHostname(APP_NAME);

  // authentication string
  ArduinoOTA.setPassword(OTA_PASSWORD);

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.print("Start updating ");
    Serial.println(type);
    publish_message("START UPDATING FIRMWARE");
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
    publish_message("DONE UPDATING FIRMWARE");
    delay(1000);
    ESP.restart();
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();
  Serial.println("OTA Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup_MQTT() {
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  //mqttClient.setCredentials("MQTT_USERNAME", "MQTT_PASSWORD");
  mqttClient.setKeepAlive(15); // seconds
  mqttClient.setClientId(APP_NAME);
  mqttClient.setWill(MQTT_TOPIC, 2, true, "DISCONNECTED");
  Serial.println("Connecting to MQTT broker...");
  mqttClient.connect();
}

void loop() {
  ArduinoOTA.handle();
}

void onMqttConnect(bool sessionPresent) {
  mqttClient.publish(MQTT_TOPIC, 1, true, "CONNECTED");
  Serial.println("Connected to the MQTT broker.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
  mqttClient.subscribe("home/entrance-wardrobe/setlightlevel", 2);
  mqttClient.subscribe("home/entrance-wardrobe/getlightlevel", 0);
}

void publish_message(String message, std::string subtopic) {
  if (mqttClient.connected()) {
    std::string topic = MQTT_TOPIC;

    if (subtopic.length() > 0) {
      // make sure subtopics always begins with an slash ("/")
      if (subtopic.find("/") > 0) {
        subtopic = "/" + subtopic;
      }

      topic = topic + subtopic;
    }

    mqttClient.publish(topic.c_str(), 2, true, message.c_str());
  }
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.print("Disconnected from the MQTT broker! reason: ");
  Serial.println(static_cast<uint8_t>(reason));
  Serial.println("Reconnecting to MQTT...");
  delay(500);
  mqttClient.connect();
}

void onMqttPublish(uint16_t packetId) {
  Serial.println("MQTT Publish acknowledged");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  Serial.println(topic);
  if (strcmp(topic,"home/entrance-wardrobe/setlightlevel") == 0) {
    setLightLevel(atoi(payload));
  } else if (strcmp(topic,"home/entrance-wardrobe/getlightlevel") == 0) {
    getLightLevel();
  } else {
    Serial.print("unknown message received. topic=");
    Serial.print(topic);
    Serial.print(", payload:");
    Serial.println(payload);
  }
}
