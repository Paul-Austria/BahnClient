#include <ESP8266WiFi.h>
#include "PubSubClient.h"
#include <ArduinoJson.h>

// --- CONFIGURATION ---
const char* ssid = "H338A_2AC3_2.4G";
const char* password = "ucgfFyGgK2CX";

const char* mqtt_server = "192.168.31.123"; 
const int   mqtt_port = 1883;
const char* topic_command = "trains/command/#"; 

WiFiClient espClient;
PubSubClient client(espClient);

// Increased buffer size slightly to be safe
DynamicJsonDocument doc(2048);

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA); 
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  // Debug print
  Serial.println("Ms got");

  DeserializationError error = deserializeJson(doc, payload, length);

  if (error) {
    Serial.print(F("JSON Error: "));
    Serial.println(error.f_str());
    return;
  }

  if (doc.containsKey("Power")) {
    bool powerOn = doc["Power"];
    
    if (powerOn) {
      Serial1.print("<1>"); 
      Serial.println("Sent Power ON: <1>");
    } else {
      Serial1.print("<0>"); 
      Serial.println("Sent Power OFF: <0>");
    }
    
    return; 
  }

  // --- EXISTING: HANDLE TRAIN COMMANDS ---
  // Only runs if "Power" was NOT in the JSON
  int address = doc["Address"] | 3; 
  int speed = doc["Speed"] | 0;         
  int direction = doc["Direction"] | 1; 

  Serial1.printf("<t 1 %d %d %d>\n", address, speed, direction);
  Serial.printf("Sent <t 1 %d %d %d>\n", address, speed, direction);

  if (doc.containsKey("Functions")) {
    JsonArray functions = doc["Functions"];
    for (JsonObject f : functions) {
      int fIndex = f["FIndex"];
      bool isActive = f["IsActive"];
      Serial1.printf("<f %d %d %d>\n", address, fIndex, isActive ? 1 : 0);
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting MQTT...");
    String clientId = "ESP8266-" + String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.subscribe(topic_command);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      delay(2000); // Shorter delay to retry faster
    }
  }
}

void setup() {
  // Debug Serial (USB)
  Serial.begin(115200);
  
  // DCC-EX Hardware Serial (Pin D4)
  // 115200 is standard for DCC-EX
  Serial1.begin(115200); 

  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  
  // Larger buffer for MQTT packets
  client.setBufferSize(512);
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();
}