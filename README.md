# ESP32 Weather Clock Terminal

A desktop weather and clock terminal built with an **ESP32** development board and a **7-pin SPI OLED display**. It connects to Wi-Fi, synchronizes Los Angeles time over NTP with automatic daylight saving time, and fetches live Santa Clara weather from the free Open-Meteo API. No weather API key is required.

## Features

- Large `HH:MM` clock synchronized over NTP
- `America/Los_Angeles` timezone with automatic daylight saving time
- Santa Clara temperature, conditions, humidity, and wind speed
- Weather refresh every 10 minutes and automatic retry after failures
- Automatic Wi-Fi reconnect
- Native 16x16 Chinese weather glyphs
- SH1106 transfer mode verified on the Inland 1.3-inch SPI OLED

## Hardware

| Part | Notes |
|---|---|
| ESP32 development board | ESP32-WROOM based board |
| 7-pin SPI OLED | 128x64, pins: GND VCC CLK MOSI RES DC CS |
| Jumper wires | 7 female-to-female Dupont wires |

## Wiring

| OLED pin | ESP32 pin |
|---|---|
| GND | GND |
| VCC | 3V3 - do not connect to VIN/5V |
| CLK | D18 / GPIO18 |
| MOSI | D23 / GPIO23 |
| RES | RX2 / GPIO16 |
| DC | TX2 / GPIO17 |
| CS | D5 / GPIO5 |

## Setup

1. Install the ESP32 board package in Arduino IDE.
2. Install `Adafruit GFX Library`, `Adafruit SSD1306`, and `ArduinoJson` using Library Manager.
3. Open `oled_weather_clock/oled_weather_clock.ino`.
4. Replace `YourWiFiName` and `YourWiFiPassword` locally. Never commit real credentials.
5. Select **ESP32 Dev Module** and upload at 115200 baud.

ESP32 supports 2.4 GHz Wi-Fi; a 5 GHz-only network will not connect.

## Display layout

```text
       14:05
10/01 Thu
68F  [weather]
Hum 41% Wind 7 mph
```

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| Screen stays unchanged | Confirm the SH1106 mode is enabled and check CS/DC/RES wiring |
| Screen stays black | Check VCC is 3V3 and verify CLK/MOSI wiring |
| Garbled or shifted image | Confirm the OLED controller; this project is configured for SH1106 |
| Stuck on Wi-Fi retry | Check SSID/password and ensure 2.4 GHz is available |
| Time is wrong | Wait for NTP and verify the timezone string |
| Weather stays on updating | Check Wi-Fi and the Open-Meteo endpoint |

## License

MIT
