// ESP32 + Inland 1.3" 7-pin SPI OLED weather clock
// Los Angeles time (automatic DST) + Santa Clara current weather

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------- Fill in only these two values ----------
const char* WIFI_SSID = "YourWiFiName";
const char* WIFI_PASS = "YourWiFiPassword";

// ---------- Fixed, verified wiring: do not change ----------
#define OLED_CLK   18
#define OLED_MOSI  23
#define OLED_RES   16
#define OLED_DC    17
#define OLED_CS     5

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64

// Default requested driver. If this particular 1.3" module is shifted or blank,
// change only 0 to 1 to use the common SH1106 page/column transfer mode.
#define OLED_DRIVER_SH1106 1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI,
                         OLED_DC, OLED_RES, OLED_CS);
SPISettings oledSPI(8000000, MSBFIRST, SPI_MODE0);

const char* WEATHER_URL =
  "https://api.open-meteo.com/v1/forecast"
  "?latitude=37.35&longitude=-121.95"
  "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m"
  "&temperature_unit=fahrenheit&wind_speed_unit=mph"
  "&timezone=America%2FLos_Angeles&forecast_days=1";

const char* LA_TZ = "PST8PDT,M3.2.0/2,M11.1.0/2";
const unsigned long WEATHER_INTERVAL_MS = 10UL * 60UL * 1000UL;
const unsigned long WEATHER_RETRY_MS = 30UL * 1000UL;
const unsigned long WIFI_RETRY_MS = 10UL * 1000UL;

float weatherTempF = 0;
float weatherWindMph = 0;
int weatherHumidity = 0;
int weatherCode = -1;
bool weatherOK = false;
bool lastWeatherFetchOK = false;
unsigned long lastWeatherAttempt = 0;
unsigned long lastWeatherSuccess = 0;
unsigned long lastWifiAttempt = 0;
unsigned long lastDraw = 0;

// 16x16 monochrome Chinese glyphs used by the weather labels.
const uint8_t PROGMEM CN_SUN[32] = {0x00,0x40,0x7B,0xF8,0x48,0x40,0x4B,0xF8,0x48,0x40,0x4F,0xFC,0x78,0x00,0x4B,0xF0,0x4B,0xF0,0x4A,0x10,0x7B,0xF0,0x4A,0x10,0x42,0x30,0x00,0x00,0x00,0x00,0x00,0x00};
const uint8_t PROGMEM CN_MANY[32] = {0x00,0x00,0x02,0x00,0x07,0x08,0x0F,0xF0,0x18,0x20,0x34,0x40,0x03,0x80,0x3D,0x80,0x33,0xFC,0x0E,0x18,0x39,0x10,0x00,0xE0,0x03,0xC0,0x7E,0x00,0x00,0x00,0x00,0x00};
const uint8_t PROGMEM CN_CLOUD[32] = {0x00,0x00,0x00,0x00,0x1F,0xF0,0x00,0x00,0x00,0x00,0x00,0x00,0x7F,0xFC,0x03,0x00,0x02,0x00,0x06,0x40,0x0C,0x60,0x08,0x30,0x3F,0xF8,0x00,0x10,0x00,0x00,0x00,0x00};
const uint8_t PROGMEM CN_OVERCAST[32] = {0x00,0x00,0x3D,0xF8,0x25,0x08,0x29,0x08,0x29,0xF8,0x29,0x08,0x29,0x08,0x29,0x08,0x25,0xF8,0x25,0x08,0x39,0x08,0x23,0x08,0x22,0x38,0x26,0x18,0x00,0x00,0x00,0x00};
const uint8_t PROGMEM CN_FOG[32] = {0x00,0x00,0x3F,0xF8,0x01,0x00,0x7F,0xFC,0x41,0x04,0x5D,0x74,0x04,0x00,0x1F,0xF0,0x24,0x40,0x0F,0xFC,0x7B,0x3C,0x03,0x00,0x3E,0xF0,0x3C,0x70,0x38,0x00,0x00,0x00};
const uint8_t PROGMEM CN_DRIZZLE[32] = {0x00,0x00,0x00,0x20,0x01,0xF0,0x3F,0x00,0x02,0x00,0x02,0x60,0x3F,0x90,0x02,0x00,0x02,0x78,0x7F,0x80,0x42,0x00,0x02,0x08,0x02,0x18,0x03,0xF8,0x00,0x00,0x00,0x00};
const uint8_t PROGMEM CN_RAIN[32] = {0x00,0x00,0x7F,0xFC,0x01,0x00,0x01,0x00,0x3F,0xF8,0x21,0x48,0x2D,0x68,0x27,0x28,0x21,0x48,0x2D,0x68,0x25,0x28,0x21,0x08,0x21,0x38,0x00,0x18,0x00,0x00,0x00,0x00};
const uint8_t PROGMEM CN_SNOW[32] = {0x00,0x00,0x3F,0xF8,0x01,0x00,0x7F,0xF8,0x5D,0x78,0x41,0x08,0x1D,0x70,0x01,0x00,0x1F,0xF0,0x00,0x10,0x7F,0xFC,0x00,0x10,0x3F,0xF0,0x00,0x10,0x00,0x00,0x00,0x00};
const uint8_t PROGMEM CN_SHOWER[32] = {0x00,0x00,0x00,0xC0,0x7C,0x80,0x4F,0xF8,0x51,0x00,0x53,0x80,0x52,0x40,0x57,0xF8,0x50,0x40,0x48,0x40,0x4F,0xF8,0x70,0x40,0x40,0x40,0x40,0x40,0x00,0x40,0x00,0x00};
const uint8_t PROGMEM CN_THUNDER[32] = {0x00,0x00,0x1F,0xF0,0x01,0x00,0x7F,0xFC,0x5D,0x74,0x41,0x00,0x1D,0x70,0x00,0x00,0x3F,0xF0,0x21,0x10,0x3F,0xF0,0x21,0x10,0x3F,0xF0,0x20,0x10,0x00,0x00,0x00,0x00};
const uint8_t PROGMEM CN_STORM[32] = {0x00,0x00,0x1F,0xF0,0x11,0xF0,0x1E,0x10,0x1F,0xF0,0x05,0xC0,0x3E,0x78,0x7F,0xFC,0x0E,0x20,0x19,0x5C,0x65,0x60,0x07,0xC0,0x39,0x38,0x03,0x00,0x00,0x00,0x00,0x00};

void oledCommand(uint8_t value) {
  SPI.beginTransaction(oledSPI);
  digitalWrite(OLED_DC, LOW);
  digitalWrite(OLED_CS, LOW);
  SPI.transfer(value);
  digitalWrite(OLED_CS, HIGH);
  SPI.endTransaction();
}

void initSH1106() {
  const uint8_t sequence[] = {
    0xAE,0xD5,0x80,0xA8,0x3F,0xD3,0x00,0x40,0xAD,0x8B,
    0xA1,0xC8,0xDA,0x12,0x81,0x8F,0xD9,0x22,0xDB,0x35,
    0xA4,0xA6,0xAF
  };
  for (uint8_t value : sequence) oledCommand(value);
}

void showDisplay() {
#if OLED_DRIVER_SH1106
  uint8_t* buffer = display.getBuffer();
  for (int page = 0; page < 8; ++page) {
    oledCommand(0xB0 + page);
    oledCommand(0x02);
    oledCommand(0x10);
    SPI.beginTransaction(oledSPI);
    digitalWrite(OLED_DC, HIGH);
    digitalWrite(OLED_CS, LOW);
    for (int x = 0; x < SCREEN_WIDTH; ++x) SPI.transfer(buffer[page * SCREEN_WIDTH + x]);
    digitalWrite(OLED_CS, HIGH);
    SPI.endTransaction();
  }
#else
  display.display();
#endif
}

void showStatus(const char* line1, const char* line2 = "") {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 20);
  display.println(line1);
  display.println(line2);
  showDisplay();
}

void drawGlyph(int x, int y, const uint8_t* glyph) {
  display.drawBitmap(x, y, glyph, 16, 16, SSD1306_WHITE);
}

void drawWeatherChinese(int x, int y, int code) {
  if (code == 0) { drawGlyph(x, y, CN_SUN); return; }
  if (code == 1 || code == 2) {
    drawGlyph(x, y, CN_MANY); drawGlyph(x + 17, y, CN_CLOUD); return;
  }
  if (code == 3) { drawGlyph(x, y, CN_OVERCAST); return; }
  if (code == 45 || code == 48) { drawGlyph(x, y, CN_FOG); return; }
  if (code >= 51 && code <= 57) {
    drawGlyph(x, y, CN_DRIZZLE); drawGlyph(x + 17, y, CN_DRIZZLE);
    drawGlyph(x + 34, y, CN_RAIN); return;
  }
  if (code >= 61 && code <= 67) { drawGlyph(x, y, CN_RAIN); return; }
  if (code >= 71 && code <= 77) { drawGlyph(x, y, CN_SNOW); return; }
  if (code >= 80 && code <= 82) {
    drawGlyph(x, y, CN_SHOWER); drawGlyph(x + 17, y, CN_RAIN); return;
  }
  if (code == 85 || code == 86) {
    drawGlyph(x, y, CN_SHOWER); drawGlyph(x + 17, y, CN_SNOW); return;
  }
  if (code >= 95) {
    drawGlyph(x, y, CN_THUNDER); drawGlyph(x + 17, y, CN_STORM); return;
  }
  display.setTextSize(1);
  display.setCursor(x, y + 4);
  display.print("?");
}

void startWifi() {
  lastWifiAttempt = millis();
  WiFi.disconnect(false);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

bool fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) {
    lastWeatherFetchOK = false;
    return false;
  }

  HTTPClient http;
  http.setTimeout(8000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(WEATHER_URL)) {
    lastWeatherFetchOK = false;
    return false;
  }

  int status = http.GET();
  if (status != HTTP_CODE_OK) {
    Serial.printf("Weather HTTP error: %d\n", status);
    http.end();
    lastWeatherFetchOK = false;
    return false;
  }

#if ARDUINOJSON_VERSION_MAJOR >= 7
  JsonDocument doc;
#else
  DynamicJsonDocument doc(2048);
#endif
  String payload = http.getString();
  http.end();
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.printf("Weather JSON error: %s\n", error.c_str());
    lastWeatherFetchOK = false;
    return false;
  }

  JsonVariant current = doc["current"];
  if (current.isNull() || current["temperature_2m"].isNull() ||
      current["relative_humidity_2m"].isNull() ||
      current["weather_code"].isNull() || current["wind_speed_10m"].isNull()) {
    Serial.println("Weather JSON missing current fields");
    lastWeatherFetchOK = false;
    return false;
  }

  weatherTempF = current["temperature_2m"].as<float>();
  weatherHumidity = current["relative_humidity_2m"].as<int>();
  weatherCode = current["weather_code"].as<int>();
  weatherWindMph = current["wind_speed_10m"].as<float>();
  weatherOK = true;
  lastWeatherFetchOK = true;
  lastWeatherSuccess = millis();
  Serial.printf("Weather: %.1fF, %d%%, code %d, %.1f mph\n",
                weatherTempF, weatherHumidity, weatherCode, weatherWindMph);
  return true;
}

void drawClock() {
  struct tm timeInfo;
  if (!getLocalTime(&timeInfo, 20)) {
    showStatus("NTP syncing...", WiFi.status() == WL_CONNECTED ? "WiFi connected" : "WiFi reconnecting");
    return;
  }

  char timeText[6];
  char dateText[13];
  char tempText[8];
  char detailText[25];
  strftime(timeText, sizeof(timeText), "%H:%M", &timeInfo);
  strftime(dateText, sizeof(dateText), "%m/%d %a", &timeInfo);

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);

  display.setTextSize(3);
  display.setCursor(19, 0);
  display.print(timeText);

  display.setTextSize(1);
  display.setCursor(0, 25);
  display.print(dateText);
  display.drawFastHLine(0, 34, SCREEN_WIDTH, SSD1306_WHITE);

  if (weatherOK) {
    snprintf(tempText, sizeof(tempText), "%dF", (int)roundf(weatherTempF));
    display.setTextSize(2);
    display.setCursor(0, 37);
    display.print(tempText);
    int weatherX = strlen(tempText) * 12 + 5;
    drawWeatherChinese(weatherX, 36, weatherCode);

    snprintf(detailText, sizeof(detailText), "Hum %d%% Wind %.0f mph",
             weatherHumidity, weatherWindMph);
    display.setTextSize(1);
    display.setCursor(0, 56);
    display.print(detailText);
  } else {
    display.setTextSize(1);
    display.setCursor(0, 43);
    display.print("weather updating...");
  }
  showDisplay();
}

void setup() {
  Serial.begin(115200);
  Serial.println("[BOOT] Weather clock starting");
  SPI.begin(OLED_CLK, -1, OLED_MOSI, OLED_CS);
  Serial.println("[BOOT] SPI initialized");

  // periphBegin=false preserves the exact SPI.begin pin assignment above.
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0, true, false)) {
    Serial.println("OLED allocation/init failed");
    while (true) delay(1000);
  }
  Serial.println("[BOOT] OLED initialized");
#if OLED_DRIVER_SH1106
  initSH1106();
#endif
  display.clearDisplay();
  showDisplay();

  showStatus("Connecting WiFi...", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  startWifi();
  configTzTime(LA_TZ, "pool.ntp.org", "time.nist.gov");

  unsigned long started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < 15000UL) delay(250);

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected: " + WiFi.localIP().toString());
    showStatus("Getting weather...", "Santa Clara");
    lastWeatherAttempt = millis();
    fetchWeather();
  } else {
    Serial.println("WiFi not connected; background retry enabled");
    showStatus("WiFi retrying...", "Check SSID/password");
  }
  drawClock();
}

void loop() {
  unsigned long now = millis();

  if (WiFi.status() != WL_CONNECTED && now - lastWifiAttempt >= WIFI_RETRY_MS) {
    startWifi();
  }

  unsigned long weatherDelay = lastWeatherFetchOK ? WEATHER_INTERVAL_MS : WEATHER_RETRY_MS;
  if (WiFi.status() == WL_CONNECTED && now - lastWeatherAttempt >= weatherDelay) {
    lastWeatherAttempt = now;
    fetchWeather();
  }

  if (now - lastDraw >= 1000UL) {
    lastDraw = now;
    drawClock();
  }
  delay(10);
}
