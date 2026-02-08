/*
 * Projekt: Zegar OLED z czujnikiem AHT10 dla ESP32-C3 Super Mini (V4)
 *
 * USTAWIENIA ARDUINO IDE:
 * - Board: "ESP32C3 Dev Module"
 * - USB CDC On Boot: "Enabled"
 * - Flash Mode: "DIO"
 *
 * ZMIANY:
 * - Ekran obrocony o 180 stopni (setRotation(3))
 * - Zegar 24h (bez AM/PM)
 * - Wylaczony WiFi Power Save (pomoc w widocznosci AP)
 * - Skaner I2C na starcie (do sprawdzenia AHT10)
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

// Konfiguracja ekranu
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// Konfiguracja pinow
#define I2C_SDA 8
#define I2C_SCL 9
#define LED_PIN 0

// Ustawienia MQTT
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
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
        if (mqttClient.connect(clientId.c_str())) {
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
    display.setRotation(3); // Obrocone o 180 (bylo 1)

    display.setCursor(0, 0);
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.println(" /////");

    // Zegar 24h
    display.setTextSize(2);
    display.setCursor(4, 25);
    if(timeClient.getHours() < 10) display.print("0");
    display.print(timeClient.getHours());

    display.setCursor(4, 45);
    if(timeClient.getMinutes() < 10) display.print("0");
    display.print(timeClient.getMinutes());

    display.setCursor(4, 65);
    if(timeClient.getSeconds() < 10) display.print("0");
    display.print(timeClient.getSeconds());

    display.drawLine(5, 85, 27, 85, SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(4, 95);
    if (sensorFound) {
        display.print((int)temperature);
        display.print("C");
    } else {
        display.print("--C");
    }

    display.setCursor(4, 110);
    if (sensorFound) {
        display.print((int)humidity);
        display.print("%");
    } else {
        display.print("--%");
    }

    display.display();
}

void scanI2C() {
    Serial.println("Skanowanie I2C...");
    byte error, address;
    int nDevices = 0;
    for (address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();
        if (error == 0) {
            Serial.print("Urzadzenie I2C na adresie 0x");
            if (address < 16) Serial.print("0");
            Serial.println(address, HEX);
            nDevices++;
        }
    }
    if (nDevices == 0) Serial.println("Nie znaleziono urzadzen I2C\n");
    else Serial.println("Skanowanie zakonczone\n");
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000);
    delay(500);

    Serial.println("\n\n--- ESP32-C3 Super Mini Start (V4) ---");
    Serial.flush();

    String chipId = String((uint32_t)ESP.getEfuseMac(), HEX);
    mqtt_topic_temp = "esp32c3/" + chipId + "/temperature";
    mqtt_topic_hum = "esp32c3/" + chipId + "/humidity";

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    Wire.begin(I2C_SDA, I2C_SCL);
    delay(100);
    scanI2C();

    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("Blad SSD1306!"));
    }

    display.clearDisplay();
    display.setRotation(3);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println("Connecting...");
    display.display();

    if (!aht.begin()) {
        Serial.println("Blad AHT10! Proba ponowna...");
        delay(1000);
        if (!aht.begin()) {
            Serial.println("Nadal nie znaleziono AHT10!");
            sensorFound = false;
        } else {
            sensorFound = true;
        }
    } else {
        sensorFound = true;
    }

    // WiFi Power Save Off dla lepszego AP
    WiFi.setSleep(false);
    WiFi.mode(WIFI_AP_STA);

    WiFiManager wm;
    wm.setConfigPortalTimeout(300);

    bool res = wm.autoConnect("ESP32C3-Setup");

    if(!res) {
        Serial.println("WiFi: Timeout portalu.");
    } else {
        Serial.println("WiFi: Polaczono!");
    }

    timeClient.begin();
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.setBufferSize(512);
    Serial.println("Setup gotowy.");
    Serial.flush();
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
