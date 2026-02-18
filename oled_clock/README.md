# ESP32-C3 Super Mini OLED Clock & AHT10 (V11)

Oprogramowanie dla ESP32-C3 Super Mini z ekranem OLED 0.91" i czujnikiem AHT10.

## Funkcje
- **Wyświetlacz:** Pionowa orientacja, zegar (NTP) + Temperatura i Wilgotność.
- **Sensor:** AHT10 (odczyt co 1s).
- **WiFi:** WiFiManager (tryb AP do konfiguracji sieci).
- **MQTT:** Integracja z Home Assistant (Discovery).
- **Web UI:** Panel pod adresem IP urządzenia do konfiguracji sprzętu i MQTT.
- **Czas:** Możliwość ustawienia strefy czasowej (UTC Offset) oraz formatu 12h/24h.
- **Zarządzanie Energią:** Konfigurowalny wygaszacz ekranu.
- **LED:** Miganie na pinie 0.

## Połączenia (Hardware)
- **OLED & AHT10 (I2C):**
  - SDA -> **Pin 10**
  - SCL -> **Pin 9**
- **LED:**
  - Pin 0

## Konfiguracja (Web Panel)
Dostępne ustawienia:
- **Strefa Czasowa:** Offset w godzinach od UTC (np. 1 dla Polski).
- **Format Czasu:** Wybór między 24h a 12h (AM/PM).
- Serwer MQTT i parametry połączenia.
- Częstotliwość raportów MQTT.
- Offset temperatury.
- Cykl pracy wyświetlacza.

## Wersja V11 - Co nowego?
- Dodano konfigurację strefy czasowej i formatu godziny (12h/24h).
- Zoptymalizowano odświeżanie czasu na ekranie.
- Dodano etykietę AM/PM w trybie 12h.
