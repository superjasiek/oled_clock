# Zegar OLED i Czujnik AHT10 dla ESP32-C3 Super Mini

Oprogramowanie dla ESP32-C3 z ekranem OLED 0.91" i czujnikiem AHT10.

## Instrukcja konfiguracji Arduino IDE

Aby poprawnie skompilować i wgrać program na **ESP32-C3 Super Mini**, wykonaj poniższe kroki:

### 1. Ustawienia Płytki (Board)
W menu **Tools** (Narzędzia) ustaw:
- **Board**: "ESP32C3 Dev Module"
- **USB CDC On Boot**: **Enabled** (Kluczowe dla Serial Monitora przez USB!)
- **Flash Mode**: "QIO" (lub "DIO")
- **Flash Size**: "4MB"

### 2. Ostrzeżenie kompilacji (Framework Warning)
Podczas kompilacji możesz zobaczyć ostrzeżenie:
`warning: 'return' with no value, in function returning non-void` w pliku `esp32-hal-uart.c`.
**Jest to znany błąd w oficjalnym frameworku Espressif i jest on całkowicie nieszkodliwy.** Program będzie działał poprawnie mimo tego komunikatu.

### 3. Debugowanie (Serial Monitor)
1. Podłącz urządzenie przez USB.
2. Otwórz **Serial Monitor** i ustaw prędkość **115200**.
3. Naciśnij przycisk **RESET** na płytce.
4. Dzięki opcji "USB CDC On Boot" oraz dodanemu w kodzie oczekiwaniu (`while(!Serial)`), logi startowe powinny pojawić się natychmiast po otwarciu monitora.

## Funkcje programu
- **WiFiManager**: Rozgłasza sieć AP **ESP32C3-Setup**, jeśli nie może się połączyć z WiFi.
- **Płynny zegar**: Odświeżanie co 1 sekundę.
- **Unikalność**: Każde urządzenie ma własne tematy MQTT oparte na Chip ID (np. `esp32c3/a1b2c3/temperature`).
- **Pionowy Layout**: Zoptymalizowany pod ekran 0.91" (32x128).
