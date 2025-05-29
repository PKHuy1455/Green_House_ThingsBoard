#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define DHTPIN 4        // Chân DHT11
#define DHTTYPE DHT11   // Loại cảm biến
#define MQ2PIN 34       // Chân MQ-2
#define SOILPIN 35      // Chân cảm biến độ ẩm đất
#define PUMP_RELAY_PIN 19  // Chân relay máy bơm
#define FAN_RELAY_PIN 18  // Chân relay quạt

DHT dht(DHTPIN, DHTTYPE);

// WiFi và ThingsBoard config
const char* ssid = "iPhoneH";
const char* password = "11111111";
const char* mqtt_server = "app.coreiot.io";
const char* access_token = "D3Th8sMArHMqKD9grvLO";

// Biến toàn cục
int mode = 0; // 0: thủ công, 1: tự động
bool pumpState = false;
bool fanState = false;
bool subscribed = false;

WiFiClient espClient;
PubSubClient client(espClient);

// Hàm RPC
void setMode(const JsonVariantConst &data, JsonDocument &response) {
  mode = data.as<int>() == 1 ? 1 : 0;
  response["mode"] = mode;
}

void setPumpState(const JsonVariantConst &data, JsonDocument &response) {
  if (mode == 0) {
    pumpState = data.as<int>() == 1 ? true : false;
    digitalWrite(PUMP_RELAY_PIN, pumpState ? LOW : HIGH);
  }
  response["pumpState"] = pumpState;
}

void setFanState(const JsonVariantConst &data, JsonDocument &response) {
  if (mode == 0) {
    fanState = data.as<int>() == 1 ? true : false;
    digitalWrite(FAN_RELAY_PIN, fanState ? LOW : HIGH);
  }
  response["fanState"] = fanState;
}

void callback(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<200> doc;
  deserializeJson(doc, payload, length);

  StaticJsonDocument<200> response;
  String method = doc["method"].as<String>();

  if (method == "setMode") {
    setMode(doc["params"], response);
  } else if (method == "setPumpState") {
    setPumpState(doc["params"], response);
  } else if (method == "setFanState") {
    setFanState(doc["params"], response);
  }

  char buffer[256];
  serializeJson(response, buffer);
  String responseTopic = String(topic);
  responseTopic.replace("request", "response");
  client.publish(responseTopic.c_str(), buffer);
}

void setup() {
  Serial.begin(115200);
  Se
  dht.begin();
  pinMode(MQ2PIN, INPUT);
  pinMode(SOILPIN, INPUT);
  pinMode(PUMP_RELAY_PIN, OUTPUT);
  pinMode(FAN_RELAY_PIN, OUTPUT);
  digitalWrite(PUMP_RELAY_PIN, HIGH); // Tắt relay
  digitalWrite(FAN_RELAY_PIN, HIGH);  // Tắt relay

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void reconnect() {
  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
    if (client.connect("ESP32_Sensors", access_token, NULL)) {
      Serial.println("Connected to MQTT");
      if (!subscribed) {
        client.subscribe("v1/devices/me/rpc/request/+");
        Serial.println("RPC subscribed");
        subscribed = true;
      }
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Đọc dữ liệu cảm biến
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  int gasValue = analogRead(MQ2PIN);
  float gasPPM = gasValue * (1000.0 / 4095.0); // Chuyển đổi sang ppm
  int soilValue = analogRead(SOILPIN);
  float soilMoisture = map(soilValue, 0, 4095, 0, 100); // Chuyển đổi sang %



  // Gửi telemetry
  StaticJsonDocument<200> doc;
  doc["temperature"] = temp;
  doc["humidity"] = hum;
  doc["gas"] = gasPPM;
  doc["soil_moisture"] = soilMoisture;
  doc["mode"] = mode;
  doc["pumpState"] = pumpState;
  doc["fanState"] = fanState;

  char buffer[256];
  serializeJson(doc, buffer);
  client.publish("v1/devices/me/telemetry", buffer);
  Serial.println("Data sent: " + String(buffer));

  delay(2000); // Gửi mỗi 60 giây
}