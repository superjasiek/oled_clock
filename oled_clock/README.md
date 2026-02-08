# Zegar OLED i Czujnik AHT10 dla ESP32-C3 Super Mini

Oprogramowanie dla ESP32-C3 z ekranem OLED 0.91" i czujnikiem AHT10.

## Instrukcja konfiguracji Arduino IDE

Aby poprawnie skompilować i wgrać program na **ESP32-C3 Super Mini**, wykonaj poniższe kroki:

### 1. Ustawienia Płytki (Board)
W menu **Tools** (Narzędzia) ustaw:
- **Board**: "ESP32C3 Dev Module"
- **USB CDC On Boot**: **Enabled** (To jest kluczowe, aby widzieć komunikaty w Serial Monitorze!)
- **Flash Mode**: "QIO" (lub "DIO" jeśli "QIO" nie działa)
- **Flash Size**: "4MB"
- **Partition Scheme**: "Default 4MB with spiffs"

### 2. Biblioteki
Upewnij się, że masz zainstalowane poniższe biblioteki (przez Library Manager):
- **Adafruit SSD1306**
- **Adafruit GFX Library**
- **Adafruit AHTX0**
- **WiFiManager** (autor: tzapu)
- **PubSubClient** (autor: knolleary)
- **NTPClient** (autor: Arduino)
- **Adafruit BusIO**

### 3. Połączenie i Debugowanie
1. Podłącz urządzenie przez USB.
2. Wybierz odpowiedni Port w Arduino IDE.
3. Otwórz **Serial Monitor** i ustaw prędkość **115200**.
4. Jeśli po wgraniu nic nie widać, naciśnij przycisk **RESET** na płytce. Dzięki ustawieniu "USB CDC On Boot: Enabled" powinieneś zobaczyć logi startowe po około 2 sekundach od startu.

## Funkcje programu
- **WiFiManager**: Przy pierwszym starcie (lub gdy nie ma zasięgu) pojawi się sieć AP o nazwie **ESP32C3-Setup**. Połącz się z nią telefonem, aby podać dane do swojego WiFi.
- **Home Assistant**: Urządzenie automatycznie wysyła dane do brokera MQTT (domyślnie broker.hivemq.com) i rejestruje się w Home Assistant przez MQTT Discovery.
- **Wyświetlacz**: Pionowy układ godziny i danych z czujnika.
- **LED**: Dioda na pinie 0 miga krótko co 10 sekund.
