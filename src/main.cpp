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
#include <WebServer.h>

// Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// Pins - Updated to SDA 10 as confirmed working in V6.2
#define I2C_SDA 10
#define I2C_SCL 9
#define LED_PIN 0

// Configuration Variables
char mqtt_server[40] = "broker.hivemq.com";
int mqtt_port = 1883;
char mqtt_user[20] = "";
char mqtt_pass[20] = "";
int led_interval = 10000;
int display_on_min = 2;
int display_period_min = 10;
float temp_offset = 0.0;
int mqtt_report_min = 20;

String mqtt_topic_temp;
String mqtt_topic_hum;

// Global objects
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_AHTX0 aht;
WiFiClient espClient;
PubSubClient mqttClient(espClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600);
WebServer server(80);

unsigned long lastSensorRead = 0;
unsigned long lastLedBlink = 0;
unsigned long lastMqttPublish = 0;
unsigned long lastAhtRetry = 0;
const long sensorInterval = 1000;

float temperature = 0;
float humidity = 0;
bool sensorFound = false;
bool displayOn = true;
String i2c_debug = "No scan performed";

// Persistence
void loadConfig() {
    if (LittleFS.begin(true)) {
        if (LittleFS.exists("/config.json")) {
            File configFile = LittleFS.open("/config.json", "r");
            if (configFile) {
                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, configFile);
                if (!error) {
                    strlcpy(mqtt_server, doc["mqtt_server"] | "broker.hivemq.com", sizeof(mqtt_server));
                    mqtt_port = doc["mqtt_port"] | 1883;
                    strlcpy(mqtt_user, doc["mqtt_user"] | "", sizeof(mqtt_user));
                    strlcpy(mqtt_pass, doc["mqtt_pass"] | "", sizeof(mqtt_pass));
                    led_interval = doc["led_interval"] | 10000;
                    display_on_min = doc["display_on_min"] | 2;
                    display_period_min = doc["display_period_min"] | 10;
                    temp_offset = doc["temp_offset"] | 0.0;
                    mqtt_report_min = doc["mqtt_report_min"] | 20;
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
    doc["led_interval"] = led_interval;
    doc["display_on_min"] = display_on_min;
    doc["display_period_min"] = display_period_min;
    doc["temp_offset"] = temp_offset;
    doc["mqtt_report_min"] = mqtt_report_min;

    File configFile = LittleFS.open("/config.json", "w");
    if (configFile) {
        serializeJson(doc, configFile);
        configFile.close();
    }
}

void scanI2C() {
    i2c_debug = "";
    byte error, address;
    int nDevices = 0;
    for (address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();
        if (error == 0) {
            i2c_debug += "0x";
            if (address < 16) i2c_debug += "0";
            i2c_debug += String(address, HEX);
            i2c_debug += " ";
            nDevices++;
        }
    }
    if (nDevices == 0) i2c_debug = "No devices found";
}

// Web Server Handlers with CSS
String getPageHeader(String title) {
    String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'><meta charset='UTF-8'>";
    html += "<style>body{font-family:sans-serif;background:#f4f4f9;padding:20px;color:#333;line-height:1.6}h1{color:#444}.card{background:#fff;padding:20px;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1);max-width:400px;margin:auto}.input-group{margin-bottom:15px}label{display:block;margin-bottom:5px;font-weight:bold}input[type=text],input[type=password],input[type=number]{width:100%;padding:8px;box-sizing:border-box;border:1px solid #ddd;border-radius:4px}input[type=range]{width:100%;margin:10px 0}.btn{display:block;background:#5c67f2;color:#fff;padding:12px;text-decoration:none;border-radius:4px;border:none;cursor:pointer;width:100%;text-align:center;font-size:1em;font-weight:bold}.btn:hover{background:#4a54e1}.footer{margin-top:20px;text-align:center;font-size:0.8em;color:#888}span.val{float:right;color:#5c67f2;font-weight:bold}hr{border:0;border-top:1px solid #eee;margin:20px 0}.debug-box{background:#eee;padding:10px;font-family:monospace;font-size:0.9em;margin-top:10px;border-radius:4px}</style>";
    html += "<title>" + title + "</title></head><body><div class='card'>";
    return html;
}

void handleRoot() {
    String html = getPageHeader("ESP32-C3 Node Status");
    html += "<h1>System Status</h1>";
    html += "<p>Temperature: <b>" + String(temperature, 1) + " C</b> (Offset: " + String(temp_offset, 1) + ")</p>";
    html += "<p>Humidity: <b>" + String(humidity, 0) + " %</b></p>";
    html += "<p>AHT10 Sensor: <b>" + String(sensorFound ? "OK" : "NOT FOUND") + "</b></p>";
    html += "<p>Display State: <b>" + String(displayOn ? "ON" : "OFF") + "</b></p>";
    html += "<hr><p>I2C Devices Found:<div class='debug-box'>" + i2c_debug + "</div></p>";
    html += "<hr><a href='/config' class='btn'>Settings</a>";
    html += "<hr><div style='text-align:center'><a href='/scan'>Re-scan I2C</a></div>";
    html += "</div><div class='footer'>ESP32 IoT Node V10</div></body></html>";
    server.send(200, "text/html", html);
}

void handleScan() {
    scanI2C();
    server.sendHeader("Location", "/");
    server.send(303);
}

void handleConfig() {
    String html = getPageHeader("Configuration");
    html += "<h1>Settings</h1><form action='/save' method='POST' oninput='out_off.value=offset.value; out_led.value=led.value; out_on.value=disp_on.value; out_per.value=disp_per.value; out_mqtt.value=mqtt_freq.value'>";

    html += "<div class='input-group'><label>MQTT Server</label><input type='text' name='server' value='" + String(mqtt_server) + "' maxlength='39'></div>";
    html += "<div class='input-group'><label>MQTT Port</label><input type='number' name='port' value='" + String(mqtt_port) + "'></div>";
    html += "<div class='input-group'><label>MQTT User</label><input type='text' name='user' value='" + String(mqtt_user) + "' maxlength='19'></div>";
    html += "<div class='input-group'><label>MQTT Password</label><input type='password' name='pass' value='" + String(mqtt_pass) + "' maxlength='19'></div>";

    html += "<div class='input-group'><label>Temp Offset (C) <span class='val'><output name='out_off'>" + String(temp_offset, 1) + "</output></span></label>";
    html += "<input type='range' name='offset' min='-10' max='10' step='0.1' value='" + String(temp_offset) + "'></div>";

    html += "<div class='input-group'><label>MQTT Report (min) <span class='val'><output name='out_mqtt'>" + String(mqtt_report_min) + "</output></span></label>";
    html += "<input type='range' name='mqtt_freq' min='1' max='120' step='1' value='" + String(mqtt_report_min) + "'></div>";

    html += "<div class='input-group'><label>LED Blink (ms) <span class='val'><output name='out_led'>" + String(led_interval) + "</output></span></label>";
    html += "<input type='range' name='led' min='0' max='30000' step='1000' value='" + String(led_interval) + "'></div>";

    html += "<div class='input-group'><label>Display ON (min) <span class='val'><output name='out_on'>" + String(display_on_min) + "</output></span></label>";
    html += "<input type='range' name='disp_on' min='1' max='60' step='1' value='" + String(display_on_min) + "'></div>";

    html += "<div class='input-group'><label>Total Cycle (min) <span class='val'><output name='out_per'>" + String(display_period_min) + "</output></span></label>";
    html += "<input type='range' name='disp_per' min='0' max='60' step='1' value='" + String(display_period_min) + "'></div>";

    html += "<input type='submit' value='Save Settings' class='btn'>";
    html += "</form><div style='text-align:center;margin-top:15px'><a href='/'>Back</a></div>";
    html += "</div></body></html>";
    server.send(200, "text/html", html);
}

void handleSave() {
    if (server.hasArg("server")) strlcpy(mqtt_server, server.arg("server").c_str(), sizeof(mqtt_server));
    if (server.hasArg("port")) mqtt_port = server.arg("port").toInt();
    if (server.hasArg("user")) strlcpy(mqtt_user, server.arg("user").c_str(), sizeof(mqtt_user));
    if (server.hasArg("pass")) strlcpy(mqtt_pass, server.arg("pass").c_str(), sizeof(mqtt_pass));
    if (server.hasArg("led")) led_interval = server.arg("led").toInt();
    if (server.hasArg("disp_on")) display_on_min = server.arg("disp_on").toInt();
    if (server.hasArg("disp_per")) display_period_min = server.arg("disp_per").toInt();
    if (server.hasArg("offset")) temp_offset = server.arg("offset").toFloat();
    if (server.hasArg("mqtt_freq")) mqtt_report_min = server.arg("mqtt_freq").toInt();

    saveConfig();
    String html = getPageHeader("Success");
    html += "<h1>Saved!</h1><p>Settings stored successfully.</p>";
    html += "<a href='/' class='btn'>Back</a></div></body></html>";
    server.send(200, "text/html", html);

    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.disconnect();
}

void setupMQTTDiscovery() {
    String chipId = String((uint32_t)ESP.getEfuseMac(), HEX);
    String deviceName = "ESP32C3-Station-" + chipId;

    String tempConfigTopic = "homeassistant/sensor/" + chipId + "_T/config";
    String tempPayload = "{\"name\": \"Temperature\", \"stat_t\": \"esp32/" + chipId + "/temperature\", \"unit_of_meas\": \"°C\", \"dev_cla\": \"temperature\", \"uniq_id\": \""+ chipId + "_T\", \"dev\": {\"ids\": [\"" + chipId + "\"], \"name\": \"" + deviceName + "\"}}";

    String humConfigTopic = "homeassistant/sensor/" + chipId + "_H/config";
    String humPayload = "{\"name\": \"Humidity\", \"stat_t\": \"esp32/" + chipId + "/humidity\", \"unit_of_meas\": \"%\", \"dev_cla\": \"humidity\", \"uniq_id\": \""+ chipId + "_H\", \"dev\": {\"ids\": [\"" + chipId + "\"], \"name\": \"" + deviceName + "\"}}";

    mqttClient.publish(tempConfigTopic.c_str(), tempPayload.c_str(), true);
    mqttClient.publish(humConfigTopic.c_str(), humPayload.c_str(), true);
}

void reconnectMQTT() {
    if (!mqttClient.connected()) {
        Serial.print("Attempting MQTT connection...");
        String clientId = "ESP32C3Client-" + String(random(0xffff), HEX);

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

    for(int i=0; i<5; i++) {
        int x = 7 + i*4;
        display.drawLine(x, 1, x+3, 7, SSD1306_WHITE);
    }
    display.drawLine(0, 11, 31, 11, SSD1306_WHITE);

    display.setTextSize(2);
    display.setCursor(4, 18);
    if(timeClient.getHours() < 10) display.print("0");
    display.print(timeClient.getHours());

    display.setCursor(4, 38);
    if(timeClient.getMinutes() < 10) display.print("0");
    display.print(timeClient.getMinutes());

    display.setCursor(4, 58); // Reverted to working V6.2 coordinate
    if(timeClient.getSeconds() < 10) display.print("0");
    display.print(timeClient.getSeconds());

    display.drawLine(0, 80, 31, 80, SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(4, 88);
    if (sensorFound) {
        display.print((int)temperature); display.print("C");
    } else {
        display.print("--C");
    }

    display.setCursor(4, 102);
    if (sensorFound) {
        display.print((int)humidity); display.print("%");
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
    mqtt_topic_temp = "esp32/" + chipId + "/temperature";
    mqtt_topic_hum = "esp32/" + chipId + "/humidity";

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // I2C Pins 10 and 9 as confirmed working in V6.2
    Wire.begin(I2C_SDA, I2C_SCL);
    delay(100);

    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println("SSD1306 ERROR");
    }
    display.ssd1306_command(SSD1306_DISPLAYON);
    display.clearDisplay();
    display.setRotation(3);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0,0);
    display.setTextSize(1);
    display.println("Start...");
    display.display();

    if (!aht.begin()) {
        Serial.println("AHT10 ERROR");
        sensorFound = false;
    } else {
        Serial.println("AHT10 OK");
        sensorFound = true;
    }

    WiFi.setSleep(false);
    WiFiManager wm;
    std::vector<const char *> menu = {"wifi", "info", "sep", "restart", "exit"};
    wm.setMenu(menu);

    if(!wm.autoConnect("ESP32-Setup")) {
        Serial.println("WiFi Portal Timeout");
    }

    server.on("/", handleRoot);
    server.on("/scan", handleScan);
    server.on("/config", handleConfig);
    server.on("/save", HTTP_POST, handleSave);
    server.begin();

    timeClient.begin();
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.setBufferSize(512);

    Serial.println("Setup Complete.");
}

void loop() {
    unsigned long currentMillis = millis();
    server.handleClient();
    timeClient.update();

    // 1. Read Sensor every 1s
    if (currentMillis - lastSensorRead >= sensorInterval) {
        lastSensorRead = currentMillis;
        if (sensorFound) {
            sensors_event_t hum, temp;
            if (aht.getEvent(&hum, &temp)) {
                temperature = temp.temperature + temp_offset;
                humidity = hum.relative_humidity;
            }
        } else if (currentMillis - lastAhtRetry > 30000) {
            lastAhtRetry = currentMillis;
            if (aht.begin()) {
                sensorFound = true;
            }
        }

        if (displayOn) {
            updateDisplay();
        }
    }

    // 2. Display Power Management
    if (display_period_min > 0) {
        long currentTotalMins = (currentMillis / 60000);
        bool shouldBeOn = (currentTotalMins % display_period_min) < display_on_min;

        if (shouldBeOn != displayOn) {
            displayOn = shouldBeOn;
            if (displayOn) {
                display.ssd1306_command(SSD1306_DISPLAYON);
                updateDisplay();
            } else {
                display.ssd1306_command(SSD1306_DISPLAYOFF);
            }
        }
    } else {
        if (!displayOn) {
            displayOn = true;
            display.ssd1306_command(SSD1306_DISPLAYON);
            updateDisplay();
        }
    }

    // 3. LED Logic
    if (led_interval > 0) {
        if (currentMillis - lastLedBlink >= led_interval) {
            lastLedBlink = currentMillis;
            digitalWrite(LED_PIN, HIGH);
        }
        if (digitalRead(LED_PIN) == HIGH && currentMillis - lastLedBlink >= 100) {
            digitalWrite(LED_PIN, LOW);
        }
    } else {
        digitalWrite(LED_PIN, LOW);
    }

    // 4. MQTT Connection
    if (!mqttClient.connected() && WiFi.status() == WL_CONNECTED) {
        static unsigned long lastReconnect = 0;
        if (currentMillis - lastReconnect > 15000) {
            lastReconnect = currentMillis;
            reconnectMQTT();
        }
    } else if (mqttClient.connected()) {
        mqttClient.loop();
    }

    // 5. MQTT Reporting Logic
    unsigned long mqttIntervalMs = (unsigned long)mqtt_report_min * 60000;
    if (currentMillis - lastMqttPublish >= mqttIntervalMs) {
        lastMqttPublish = currentMillis;
        if (mqttClient.connected()) {
            mqttClient.publish(mqtt_topic_temp.c_str(), String(temperature, 1).c_str());
            mqttClient.publish(mqtt_topic_hum.c_str(), String(humidity, 1).c_str());
        }
    }
}
