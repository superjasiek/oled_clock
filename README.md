ESP32-C3 Super Mini OLED Clock & AHT10 (V10)
Oprogramowanie dla ESP32-C3 Super Mini z ekranem OLED 0.91" i czujnikiem AHT10.

Funkcje
Wyświetlacz: Pionowa orientacja, zegar (NTP) + Temperatura i Wilgotność.
Sensor: AHT10 (odczyt co 1s).
WiFi: WiFiManager (tryb AP do konfiguracji sieci).
MQTT: Integracja z Home Assistant (Discovery), konfigurowalny serwer i częstotliwość raportów.
Web UI: Panel pod adresem IP urządzenia do konfiguracji sprzętu i MQTT.
Zarządzanie Energią: Konfigurowalny wygaszacz ekranu (Duty Cycle).
LED: Miganie na pinie 0 (konfigurowalny interfejs).
Połączenia (Hardware)
OLED & AHT10 (I2C):
SDA -> Pin 10 (Ważne: Niektóre moduły Super Mini wymagają pinu 10 zamiast 8)
SCL -> Pin 9
VCC -> 3.3V
GND -> GND
LED:
Wbudowana lub zewnętrzna -> Pin 0
Konfiguracja (Web Panel)
Po połączeniu z WiFi, wejdź na adres IP urządzenia (widoczny w Serial Monitorze lub na routerze). Dostępne ustawienia:

Serwer MQTT, Port, User, Password.
Częstotliwość MQTT: Od 1 do 120 minut.
Offset Temperatury: Korekta odczytu czujnika.
Czas świecenia ekranu: Ustawienie oszczędzania wypalania OLED.
Diagnostyka I2C: Przycisk skanowania magistrali I2C.
Instalacja (Arduino IDE)
Wymagane biblioteki:
Adafruit SSD1306 & GFX
Adafruit AHTX0
WiFiManager
PubSubClient
NTPClient
ArduinoJson
Ustawienia płytki: ESP32C3 Dev Module
Ważne: Włącz USB CDC On Boot: Enabled w menu Tools, aby widzieć logi Serial.
Wersja V10 - Co nowego?
Naprawiono problem z czarnym ekranem (poprawna inicjalizacja).
Przywrócono Pin 10 dla SDA (zgodność z większością modułów C3 Super Mini).
Dodano separację odczytu sensora od sieci - zegar i temperatura odświeżają się płynnie.
Nowoczesny wygląd interfejsu WWW z suwakami.
Dodano funkcję ponownego skanowania I2C w razie problemów z połączeniem.
