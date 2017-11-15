#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <DS3231.h>
#include <ArduinoJson.h>

#define DHTTYPE DHT11
#define DHTPIN  2

const char* ssid = "Cloud_2";
const char* ssid_password = "";

const char* mqtt_server = "192.168.0.110";
const char* mqtt_device_name = "temp_hum_03";

const char* outTopic = "home/out";
const char* inTopic = "home/in";
const char* debugTopic = "home/debug";

const char* tempSensorName = "temp_sensor_03";
const char* humiditySensorName = "humidity_sensor_03";
const char* roomName = "green_bedroom";

const char* CMD_STATE = "state";
const char* NAME_ALL = "all";

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);

int humidity, temp;
int lastSentHumidity = 0, lastSentTemp = 0, lastSentWifiStrength = 0;
long wifiStrength = 0;

boolean wifi_connecting = false, wifi_connected = false, wifi_error = false;
boolean mqtt_connecting = false, mqtt_connected = false, mqtt_error = false;

void setup() {
  delay(100);
  Serial.begin(115200);
  
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  dht.begin();  
}

void loop() {
  setup_wifi();

  if (wifi_connected) {
    setup_mqtt();
  }

  bool valuesUpdated = updateValues();
  
  if (valuesUpdated && mqtt_connected) {
      sendLastValues();
  }

//  debugPrint();
  client.loop();
  delay(500);
}

void setup_mqtt() {
  mqtt_connected = client.connected();

  if (mqtt_connected) {
    return;
  }

  if (!mqtt_connecting) {
    mqtt_connecting = true;

    if (client.connect(mqtt_device_name)) {
      mqtt_error = false;
      mqtt_connecting = false;
      mqtt_connected = true;
      client.subscribe(inTopic);
    } else {
      mqtt_error = true;
      mqtt_connecting = false;
      mqtt_connected = false;
    }
  }
}

void setup_wifi() {
  wifi_connected = WiFi.status() == WL_CONNECTED;
  wifi_error = WiFi.status() == WL_CONNECT_FAILED;
  if (wifi_connected || wifi_error) {
    wifi_connecting = false;
  }
  
  if (wifi_connected) {
    return;
  }

  if (!wifi_connecting) {
    wifi_connecting = true;  
    WiFi.begin(ssid, ssid_password);    
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  if (!eq(topic, inTopic)) {
    return;
  }
  
  DynamicJsonBuffer jsonBuffer;
  char jsonMessageBuffer[256];

  JsonObject& message = jsonBuffer.parseObject(payload);

  if (!message.success())
  {
    client.publish(debugTopic, mqtt_device_name);
    client.publish(debugTopic, "cannot parse message");
    return;
  }

//  message.printTo(jsonMessageBuffer, sizeof(jsonMessageBuffer));  
//  Serial.print("<-- ");
//  Serial.println(jsonMessageBuffer);

  const char* messageName = message["name"];
  if (!eq(messageName, tempSensorName) && !eq(messageName, humiditySensorName) && !eq(messageName, NAME_ALL)) {
    return;
  }

  const char* messageCmd = message["cmd"];
  if (eq(messageCmd, CMD_STATE)) {
    sendLastValues();
  }
}


void sendLastValues() {
  sendTemp();
  sendHumidity();

  lastSentWifiStrength = wifiStrength;
}

void sendTemp() {
  DynamicJsonBuffer jsonBuffer;
  char jsonMessageBuffer[256];

  JsonObject& root = jsonBuffer.createObject();
  root["name"] = tempSensorName;
  root["type"] = "temp_sensor";
  root["room"] = roomName;
  root["signal"] = wifiStrength;
  root["state"] = 1;
  root["value"] = temp;

  root.printTo(jsonMessageBuffer, sizeof(jsonMessageBuffer));
  
  Serial.println(jsonMessageBuffer);
  client.publish(outTopic, jsonMessageBuffer);
  
  lastSentTemp = temp;
}

void sendHumidity() {
  DynamicJsonBuffer jsonBuffer;
  char jsonMessageBuffer[256];

  JsonObject& root = jsonBuffer.createObject();
  root["name"] = humiditySensorName;
  root["type"] = "humidity_sensor";
  root["room"] = roomName;
  root["signal"] = wifiStrength;
  root["state"] = 1;
  root["value"] = humidity;
  
  root.printTo(jsonMessageBuffer, sizeof(jsonMessageBuffer));
  
  Serial.println(jsonMessageBuffer);
  client.publish(outTopic, jsonMessageBuffer);
  
  lastSentHumidity = humidity;
}

bool updateValues() {
  // float to int conversion here
  int t = dht.readTemperature();
  int h = dht.readHumidity();

  if (t > 0 && t < 99) {
    temp = t;
  }

  if (h > 0 && h < 99) {
    humidity = h;
  }

  if (wifi_connected) {
    wifiStrength = WiFi.RSSI();
  } else {
    wifiStrength = 0;
  }

  return lastSentHumidity != humidity || lastSentTemp != temp || abs(lastSentWifiStrength - wifiStrength) > 3;
}

boolean eq(const char* a1, const char* a2) {
  return strcmp(a1, a2) == 0;
}

void debugPrint() {
  Serial.print("wifi connecting = ");
  Serial.println(wifi_connecting);

  Serial.print("wifi connected = ");
  Serial.println(wifi_connected);
  
  Serial.print("wifi error = ");
  Serial.println(wifi_error);

  Serial.print("mqtt connecting = ");
  Serial.println(mqtt_connecting);

  Serial.print("mqtt connected = ");
  Serial.println(mqtt_connected);
  
  Serial.print("mqtt error = ");
  Serial.println(mqtt_error);

  Serial.print("mqtt state = ");
  Serial.println(client.state());
}

