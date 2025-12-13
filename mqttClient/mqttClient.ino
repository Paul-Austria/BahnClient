#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include "PubSubClient.h"
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

const char* ssid = "H338A_2AC3_2.4G";
const char* password = "ucgfFyGgK2CX";
const char* mqttServer = "192.168.31.123";
const int mqttPort = 1883;
const char* mqttTopic1 = "station1/servo";
const char* mqttTopic2 = "station1/sw";

WiFiClient espClient;
PubSubClient client(espClient);


Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(0x40);


#define SERVOMIN  150 // This is the 'minimum' pulse length count (out of 4096)
#define SERVOMAX  480 // This is the 'maximum' pulse length count (out of 4096)
#define SERVO_FREQ 50 // Analog servos run at ~50 Hz updates

struct Switch{
  int iD;
  int pinNumber;
  char* name;
  int nameLenght;
  bool state;
  bool coSwitch;
  char* topic;
  int topicLenght;
};

struct Servo{
   int iD;
   int topic;
   int name;
   int pwmPinId;
   int degree;
};

void setup() {
  Serial.begin(19200);
  pinMode(LED_BUILTIN, OUTPUT);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  bool ledState = false;
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    digitalWrite(LED_BUILTIN, ledState); 
    ledState !=ledState;
  }
  digitalWrite(LED_BUILTIN, LOW); 
//  Serial.println("Connected to WiFi");

  // Connect to MQTT broker
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);

  while (!client.connected()) { 
    if (client.connect("ESP8266Client")) {
  //    Serial.println("Connected to MQTT broker");
      client.subscribe(mqttTopic1);
      client.subscribe(mqttTopic2);

    } else {
   //   Serial.print("Failed, rc=");
  //    Serial.print(client.state());
  //    Serial.println(" Retrying in 5 seconds...");
      delay(5000);
    }
  }
  pwm.begin();
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(SERVO_FREQ);  // Analog servos run at ~50 Hz updates


  delay(3000);
  client.publish("host/getdata", "{\"Topic\": \"station1\"}"); // update states of Servos and clients

}

void loop() {
  client.loop();
}


void SetServo(int id, int degree)
{
    int pulselen = map(degree, 0, 180, SERVOMIN, SERVOMAX);
    pwm.setPWM(id,0 , pulselen);
}


void handleServoMessage(byte* payload, unsigned int length) {
    JsonDocument doc;
    deserializeJson(doc, payload);


    Servo serv;
    serv.iD = doc["iD"];
    serv.topic = doc["topic"];
    serv.pwmPinId = doc["pwmPinId"];
    serv.degree = doc["degree"];

    if(serv.pwmPinId >0 && serv.pwmPinId < 15)
      SetServo(serv.pwmPinId, serv.degree);
    
}

void handleSwitchMessage(byte* payload, unsigned int length) {
  JsonDocument doc;
  deserializeJson(doc, payload);
  Switch sw;
  sw.iD = doc["iD"];
  sw.state = doc["state"];
  sw.pinNumber = doc["pinNumber"];
  sw.coSwitch = doc["CoSwitch"];
  


  if(sw.coSwitch){
      String combinedString = String(sw.pinNumber) + "," + sw.state+";";
      Serial.println(combinedString);
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
/*  Serial.println("Message received:");

  Serial.print("Topic: ");
  Serial.println(topic);

  Serial.print("Payload: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  */
  //Serial.println();

  // Check the topic and call the appropriate function
  if (strcmp(topic, mqttTopic1) == 0) {
    handleServoMessage(payload, length);
  } else if (strcmp(topic,mqttTopic2) == 0) {
    handleSwitchMessage(payload, length);
  } else {
 //   Serial.println("Unknown topic");
    // Handle unknown topics if needed
  }
}
