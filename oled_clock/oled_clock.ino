/*
 * Projekt: Zegar OLED z czujnikiem AHT10 dla ESP32-C3 Super Mini (V5)
 *
 * USTAWIENIA ARDUINO IDE:
 * - Board: "ESP32C3 Dev Module"
 * - USB CDC On Boot: "Enabled"
 * - Flash Mode: "DIO"
 *
 * ZMIANY V5:
 * - Konfiguracja MQTT (serwer, port, uzytkownik, haslo) przez WWW (WiFiManager)
 * - Zapis ustawien w pamieci Flash (LittleFS)
 * - Poprawiony layout OLED: 180 deg, zegar 24h, linie oddzielajace, wyrownane skosy
 * - Wylaczony WiFi Power Save
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_AHTX0.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// Konfiguracja ekranu
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// Konfiguracja pinow
#define I2C_SDA 8
#define I2C_SCL 9
#define LED_PIN 0

// Domyslne ustawienia MQTT
char mqtt_server[40] = "broker.hivemq.com";
char mqtt_port[6] = "1883";
char mqtt_user[20] = "";
char mqtt_pass[20] = "";

String mqtt_topic_temp;
String mqtt_topic_hum;

// Obiekty
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_AHTX0 aht;
WiFiClient espClient;
PubSubClient mqttClient(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600);

unsigned long lastSensorRead = 0;
unsigned long lastLedBlink = 0;
unsigned long lastMqttPublish = 0;
const long sensorInterval = 1000;
const long ledInterval = 10000;
const long mqttInterval = 60000;

float temperature = 0;
float humidity = 0;
bool sensorFound = false;
bool shouldSaveConfig = false;

// WiFiManager callback
void saveConfigCallback() {
    shouldSaveConfig = true;
}

void loadConfig() {
    if (LittleFS.begin(true)) {
        if (LittleFS.exists("/config.json")) {
            File configFile = LittleFS.open("/config.json", "r");
            if (configFile) {
                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, configFile);
                if (!error) {
                    strcpy(mqtt_server, doc["mqtt_server"] | "broker.hivemq.com");
                    strcpy(mqtt_port, doc["mqtt_port"] | "1883");
                    strcpy(mqtt_user, doc["mqtt_user"] | "");
                    strcpy(mqtt_pass, doc["mqtt_pass"] | "");
                }
                configFile.close();
            }
        }
    }
}

void saveConfig() {
    JsonDocument doc;
    doc["mqtt_server"] = mqtt_server;
    doc["mqtt_port"] = mqtt_port;
    doc["mqtt_user"] = mqtt_user;
    doc["mqtt_pass"] = mqtt_pass;

    File configFile = LittleFS.open("/config.json", "w");
    if (configFile) {
        serializeJson(doc, configFile);
        configFile.close();
    }
}

void setupMQTTDiscovery() {
    String chipId = String((uint32_t)ESP.getEfuseMac(), HEX);

    String tempConfigTopic = "homeassistant/sensor/esp32c3_" + chipId + "_T/config";
    String tempPayload = "{\"name\": \"ESP32-C3 Temperature\", \"stat_t\": \"" + mqtt_topic_temp + "\", \"unit_of_meas\": \"°C\", \"dev_cla\": \"temperature\", \"uniq_id\": \"esp32c3_" + chipId + "_T\", \"dev\": {\"ids\": [\"esp32c3_" + chipId + "\"], \"name\": \"ESP32-C3 Sensor Station\"}}";

    String humConfigTopic = "homeassistant/sensor/esp32c3_" + chipId + "_H/config";
    String humPayload = "{\"name\": \"ESP32-C3 Humidity\", \"stat_t\": \"" + mqtt_topic_hum + "\", \"unit_of_meas\": \"%\", \"dev_cla\": \"humidity\", \"uniq_id\": \"esp32c3_" + chipId + "_H\", \"dev\": {\"ids\": [\"esp32c3_" + chipId + "\"], \"name\": \"ESP32-C3 Sensor Station\"}}";

    mqttClient.publish(tempConfigTopic.c_str(), tempPayload.c_str(), true);
    mqttClient.publish(humConfigTopic.c_str(), humPayload.c_str(), true);
}

void reconnectMQTT() {
    if (!mqttClient.connected()) {
        Serial.print("Connecting to MQTT...");
        String clientId = "ESP32C3Client-";
        clientId += String(random(0xffff), HEX);

        bool connected = false;
        if (strlen(mqtt_user) > 0) {
            connected = mqttClient.connect(clientId.c_str(), mqtt_user, mqtt_pass);
        } else {
            connected = mqttClient.connect(clientId.c_str());
        }

        if (connected) {
            Serial.println("connected");
            setupMQTTDiscovery();
        } else {
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
        }
    }
}

void updateDisplay() {
    display.clearDisplay();
    display.setRotation(3);

    display.setTextColor(SSD1306_WHITE);

    // Ukosne kreski - wyrownane
    for(int i=0; i<5; i++) {
        int x = 7 + i*4;
        display.drawLine(x, 1, x+3, 7, SSD1306_WHITE);
    }

    // Gorna linia pozioma
    display.drawLine(0, 11, 31, 11, SSD1306_WHITE);

    // Zegar 24h
    display.setTextSize(2);
    display.setCursor(4, 18);
    if(timeClient.getHours() < 10) display.print("0");
    display.print(timeClient.getHours());

    display.setCursor(4, 38);
    if(timeClient.getMinutes() < 10) display.print("0");
    display.print(timeClient.getMinutes());

    display.setCursor(4, 58);
    if(timeClient.getSeconds() < 10) display.print("0");
    display.print(timeClient.getSeconds());

    // Dolna linia pozioma
    display.drawLine(0, 80, 31, 80, SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(4, 88);
    if (sensorFound) {
        display.print((int)temperature);
        display.print("C");
    } else {
        display.print("--C");
    }

    display.setCursor(4, 102);
    if (sensorFound) {
        display.print((int)humidity);
        display.print("%");
    } else {
        display.print("--%");
    }

    display.display();
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000);
    delay(500);

    loadConfig();

    String chipId = String((uint32_t)ESP.getEfuseMac(), HEX);
    mqtt_topic_temp = "esp32c3/" + chipId + "/temperature";
    mqtt_topic_hum = "esp32c3/" + chipId + "/humidity";

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    Wire.begin(I2C_SDA, I2C_SCL);
    delay(100);

    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 ERROR"));
    }

    display.clearDisplay();
    display.setRotation(3);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println("Setup...");
    display.display();

    if (!aht.begin()) {
        sensorFound = false;
    } else {
        sensorFound = true;
    }

    WiFi.setSleep(false);
    WiFi.mode(WIFI_AP_STA);

    WiFiManager wm;
    wm.setSaveConfigCallback(saveConfigCallback);

    WiFiManagerParameter custom_mqtt_server("server", "MQTT Server", mqtt_server, 40);
    WiFiManagerParameter custom_mqtt_port("port", "MQTT Port", mqtt_port, 6);
    WiFiManagerParameter custom_mqtt_user("user", "MQTT User", mqtt_user, 20);
    WiFiManagerParameter custom_mqtt_pass("pass", "MQTT Password", mqtt_pass, 20);

    wm.addParameter(&custom_mqtt_server);
    wm.addParameter(&custom_mqtt_port);
    wm.addParameter(&custom_mqtt_user);
    wm.addParameter(&custom_mqtt_pass);

    wm.setConfigPortalTimeout(300);

    if(!wm.autoConnect("ESP32C3-Setup")) {
        Serial.println("WiFi: Portal Timeout");
    } else {
        Serial.println("WiFi: Connected!");
    }

    strcpy(mqtt_server, custom_mqtt_server.getValue());
    strcpy(mqtt_port, custom_mqtt_port.getValue());
    strcpy(mqtt_user, custom_mqtt_user.getValue());
    strcpy(mqtt_pass, custom_mqtt_pass.getValue());

    if (shouldSaveConfig) {
        saveConfig();
    }

    timeClient.begin();
    mqttClient.setServer(mqtt_server, atoi(mqtt_port));
    mqttClient.setBufferSize(512);
    Serial.println("Setup Complete.");
}

void loop() {
    unsigned long currentMillis = millis();

    timeClient.update();

    if (currentMillis - lastSensorRead >= sensorInterval) {
        lastSensorRead = currentMillis;
        if (sensorFound) {
            sensors_event_t hum, temp;
            aht.getEvent(&hum, &temp);
            temperature = temp.temperature;
            humidity = hum.relative_humidity;
        }
        updateDisplay();
    }

    if (currentMillis - lastLedBlink >= ledInterval) {
        lastLedBlink = currentMillis;
        digitalWrite(LED_PIN, HIGH);
        delay(100);
        digitalWrite(LED_PIN, LOW);
    }

    if (!mqttClient.connected() && WiFi.status() == WL_CONNECTED) {
        static unsigned long lastReconnect = 0;
        if (currentMillis - lastReconnect > 15000) {
            lastReconnect = currentMillis;
            reconnectMQTT();
        }
    } else if (mqttClient.connected()) {
        mqttClient.loop();
    }

    if (currentMillis - lastMqttPublish >= mqttInterval) {
        lastMqttPublish = currentMillis;
        if (mqttClient.connected()) {
            mqttClient.publish(mqtt_topic_temp.c_str(), String(temperature).c_str());
            mqttClient.publish(mqtt_topic_hum.c_str(), String(humidity).c_str());
        }
    }
}
