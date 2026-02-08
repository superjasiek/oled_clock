/*
 * Projekt: Zegar OLED z czujnikiem AHT10 dla ESP32-C3
 *
 * Wymagane biblioteki (zainstaluj przez Library Manager w Arduino IDE):
 * - Adafruit SSD1306
 * - Adafruit GFX Library
 * - Adafruit AHTX0
 * - WiFiManager by tzapu
 * - PubSubClient by knolleary
 * - NTPClient by Arduino
 * - Adafruit BusIO
 *
 * Konfiguracja pinów:
 * - I2C SDA: Pin 8
 * - I2C SCL: Pin 9
 * - LED: Pin 0
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

// Konfiguracja ekranu
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// Konfiguracja pinów
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
const long sensorInterval = 1000; // Aktualizacja co 1s dla płynnego zegara
const long ledInterval = 10000;
const long mqttInterval = 60000;

float temperature = 0;
float humidity = 0;
bool sensorFound = false;

// Funkcja rejestrująca czujniki w Home Assistant (MQTT Discovery)
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
        Serial.print("Łączenie z MQTT...");
        String clientId = "ESP32C3Client-";
        clientId += String(random(0xffff), HEX);
        if (mqttClient.connect(clientId.c_str())) {
            Serial.println("połączono");
            setupMQTTDiscovery();
        } else {
            Serial.print("błąd, rc=");
            Serial.print(mqttClient.state());
        }
    }
}

void updateDisplay() {
    display.clearDisplay();
    display.setRotation(1); // Orientacja pionowa

    display.setCursor(0, 0);
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.println(" /////");

    String ampm = timeClient.getHours() >= 12 ? "PM" : "AM";
    display.setCursor(8, 15);
    display.print(ampm);

    display.setTextSize(2);
    // Godzina
    display.setCursor(4, 30);
    if(timeClient.getHours() < 10) display.print("0");
    display.print(timeClient.getHours());

    // Minuty
    display.setCursor(4, 50);
    if(timeClient.getMinutes() < 10) display.print("0");
    display.print(timeClient.getMinutes());

    // Sekundy
    display.setCursor(4, 70);
    if(timeClient.getSeconds() < 10) display.print("0");
    display.print(timeClient.getSeconds());

    display.drawLine(5, 90, 27, 90, SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(4, 100);
    display.print((int)temperature);
    display.print("C");

    display.setCursor(4, 115);
    display.print((int)humidity);
    display.print("%");

    display.display();
}

void setup() {
    Serial.begin(115200);

    String chipId = String((uint32_t)ESP.getEfuseMac(), HEX);
    mqtt_topic_temp = "esp32c3/" + chipId + "/temperature";
    mqtt_topic_hum = "esp32c3/" + chipId + "/humidity";

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    Wire.begin(I2C_SDA, I2C_SCL);

    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("Błąd inicjalizacji SSD1306"));
    }
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.println("Łączenie...");
    display.display();

    if (!aht.begin()) {
        Serial.println("Nie znaleziono czujnika AHT10!");
        sensorFound = false;
    } else {
        sensorFound = true;
    }

    WiFiManager wm;
    bool res = wm.autoConnect("ESP32C3-Setup");

    if(!res) {
        Serial.println("Błąd połączenia z WiFi");
    } else {
        Serial.println("Połączono z WiFi");
    }

    timeClient.begin();
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.setBufferSize(512); // Zwiększony bufor dla HA discovery
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

    if (!mqttClient.connected()) {
        static unsigned long lastReconnect = 0;
        if (currentMillis - lastReconnect > 5000) {
            lastReconnect = currentMillis;
            reconnectMQTT();
        }
    } else {
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
