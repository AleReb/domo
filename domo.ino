#include <Adafruit_ADS1X15.h>
#include <Adafruit_BME280.h>
#include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <DHT.h>
#include <DHT_U.h>
#include <RTClib.h>
#include <SD.h>
#include <SHT4x.h>
#include <SPI.h>
#include <U8g2lib.h>
#include <Wire.h>

/* ===== PIN DEFINITIONS ===== */
// SD Card SPI Configuration (HSPI)
#define PIN_NUM_MISO 2
#define PIN_NUM_MOSI 15
#define PIN_NUM_CLK 14
#define PIN_NUM_CS 13

// DHT22 Configuration
#define DHTPIN 19
#define DHTTYPE DHT22

// BME280 Configuration
#define SEALEVELPRESSURE_HPA (1013.25)

/* ===== I2C CONFIG ===== */
static const uint8_t OLED_ADDR_7BIT = 0x3C;
static const uint8_t ADS_ADDR_7BIT = 0x48;
static const uint8_t BME_ADDR_7BIT = 0x76;

/* ===== CONSTANTS ===== */
static const uint32_t LOG_INTERVAL_MS = 1000;    // 1 segundo
static const uint32_t DISPLAY_INTERVAL_MS = 500; // 1/2 second
static const uint32_t READ_INTERVAL_MS = 2000;   // 2 seconds
static const uint32_t OLED_ROTATION_MS = 20000;  // 20 seconds rotation
static const uint32_t INIT_RETRY_MS = 3000;      // retry init every 3 seconds

/* ===== GLOBAL OBJECTS ===== */
// OLED (128x64 SSD1306) chica/
// U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);
U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(U8G2_R0, /* reset=*/U8X8_PIN_NONE);
// SHT40
SHT4x sht(0x44, &Wire);

// ADS1115
Adafruit_ADS1115 ads;

// BME280
Adafruit_BME280 bme;

// DHT22
DHT_Unified dht(DHTPIN, DHTTYPE);

// RTC
RTC_DS3231 rtc;
// uptime
char upStr[32];
// SD (HSPI)
SPIClass sdSPI(HSPI);

/* ===== STATUS FLAGS ===== */
static bool oledFound = false;
static bool shtFound = false;
static bool adsFound = false;
static bool bmeFound = false;
static bool dhtFound = false;
static bool rtcFound = false;
static bool sdFound = false;

/* ===== TIMING VARIABLES ===== */
static uint32_t lastLogTime = 0;
static uint32_t lastDisplayTime = 0;
static uint32_t lastReadTime = 0;
static uint32_t lastInitTryMs = 0;

/* ===== FILE ROTATION ===== */
static int lastYMD = -1; // YYYYMMDD
static String currentFileName = "";

/* ===== SENSOR DATA ===== */
static float shtTemp = NAN, shtHum = NAN;
static float hmpTemp = NAN, hmpHum = NAN;
static float bmeTemp = NAN, bmeHum = NAN, bmePress = NAN;
static float dhtTemp = NAN, dhtHum = NAN;

/* ===================== HELPERS ===================== */
static float clampf(float x, float lo, float hi) {
  if (x < lo)
    return lo;
  if (x > hi)
    return hi;
  return x;
}

/* ===================== INIT HELPERS ===================== */
static void tryInitNonBlocking() {
  uint32_t nowMs = millis();
  if (nowMs - lastInitTryMs < INIT_RETRY_MS)
    return;
  lastInitTryMs = nowMs;

  // I2C init (only once is fine, but harmless if repeated)
  Wire.begin();
  Wire.setClock(100000);

  if (!oledFound) {
    Serial.println("[INIT] OLED begin...");
    oled.setI2CAddress(OLED_ADDR_7BIT << 1); // U8g2 expects 8-bit address
    if (oled.begin()) {
      oledFound = true;
      oled.setFont(u8g2_font_6x10_tf);
      oled.clearBuffer();
      oled.drawStr(0, 12, "OLED: OK");
      oled.sendBuffer();
      Serial.println("[OK] OLED Initialized");
    } else {
      Serial.println("[ERR] OLED NOT Found");
    }
  }

  if (!shtFound) {
    Serial.println("[INIT] SHT40 begin...");
    if (sht.begin()) {
      shtFound = true;
      Serial.println("[OK] SHT40 Found");
    } else {
      Serial.println("[ERR] SHT40 NOT Found");
    }
  }

  if (!adsFound) {
    Serial.println("[INIT] ADS1115 begin...");
    if (ads.begin(ADS_ADDR_7BIT, &Wire)) {
      adsFound = true;

      // Safe choice (works even if voltage is above 1V by mistake)
      // If you KNOW it's 0-1V, you can switch to GAIN_FOUR for better
      // resolution.
      ads.setGain(GAIN_ONE); // +/-4.096V
      Serial.println("[OK] ADS1115 Found (GAIN_ONE)");
    } else {
      Serial.println("[ERR] ADS1115 NOT Found");
    }
  }

  if (!rtcFound) {
    Serial.println("[INIT] RTC begin...");
    if (rtc.begin()) {
      rtcFound = true;
      Serial.println("[OK] RTC Found");
    } else {
      Serial.println("[ERR] RTC NOT Found");
    }
  }

  if (!bmeFound) {
    Serial.println("[INIT] BME280 begin...");
    if (bme.begin(BME_ADDR_7BIT)) {
      bmeFound = true;
      Serial.println("[OK] BME280 Found");
    } else {
      Serial.println("[ERR] BME280 NOT Found");
    }
  }

  if (!dhtFound) {
    Serial.println("[INIT] DHT22 begin...");
    dht.begin();
    // DHT_Unified doesn't return success on begin(), but we check metadata to
    // verify
    sensor_t sensor;
    dht.temperature().getSensor(&sensor);
    if (sensor.sensor_id != 0) {
      dhtFound = true;
      Serial.println("[OK] DHT22 Found");
    } else {
      Serial.println("[ERR] DHT22 NOT Found");
    }
  }

  if (!sdFound) {
    Serial.println("[INIT] SD begin...");
    sdSPI.begin(PIN_NUM_CLK, PIN_NUM_MISO, PIN_NUM_MOSI, PIN_NUM_CS);
    if (SD.begin(PIN_NUM_CS, sdSPI)) {
      sdFound = true;
      Serial.println("[OK] SD Card Mounted");
    } else {
      Serial.println("[ERR] SD Card Mount Failed");
    }
  }
}

/* ===================== SENSOR READ ===================== */
static void updateSensors() {
  if (shtFound) {
    if (sht.read()) {
      shtTemp = sht.getTemperature();
      shtHum = sht.getHumidity();
    }
  }

  if (adsFound) {
    // HMP60 analog output: 0..1000 mV
    // TempC = mV * 0.1 - 40
    // RH%   = mV * 0.1

    int16_t rawT = ads.readADC_SingleEnded(0);  // CH0
    int16_t rawRH = ads.readADC_SingleEnded(1); // CH1

    // Using volts from library, then convert to millivolts explicitly
    float vT = ads.computeVolts(rawT);
    float vRH = ads.computeVolts(rawRH);

    float mVT = vT * 1000.0f;
    float mVRH = vRH * 1000.0f;

    // Correct conversion (manual-style)
    hmpTemp = (mVT * 0.1f) - 40.0f;
    hmpHum = (mVRH * 0.1f);

    // Clamp humidity to valid range
    hmpHum = clampf(hmpHum, 0.0f, 100.0f);

    // Debug useful for calibration
    Serial.printf("[ADS RAW] CH0=%d (%.1f mV) | CH1=%d (%.1f mV)\n", rawT, mVT,
                  rawRH, mVRH);
  }

  if (bmeFound) {
    bmeTemp = bme.readTemperature();
    bmeHum = bme.readHumidity();
    bmePress = bme.readPressure() / 100.0F;
  }

  if (dhtFound) {
    sensors_event_t event;
    dht.temperature().getEvent(&event);
    if (!isnan(event.temperature)) {
      dhtTemp = event.temperature;
    } else {
      dhtTemp = NAN;
    }
    dht.humidity().getEvent(&event);
    if (!isnan(event.relative_humidity)) {
      dhtHum = event.relative_humidity;
    } else {
      dhtHum = NAN;
    }
  }
}

/* ===================== OLED UPDATE ===================== */
static void updateOLED() {
  if (!oledFound)
    return;

  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tf);

  uint32_t nowMs = millis();
  bool showScreen2 = (nowMs % (OLED_ROTATION_MS * 2)) >= OLED_ROTATION_MS;

  // common headers (Line 1 & 2)
  int y = 12;
  if (rtcFound) {
    DateTime now = rtc.now();
    char ts[32];
    snprintf(ts, sizeof(ts), "%02d/%02d/%04d %02d:%02d:%02d", now.day(),
             now.month(), now.year(), now.hour(), now.minute(), now.second());
    oled.drawStr(0, y, ts);
    Serial.printf("[%02d:%02d:%02d] ", now.hour(), now.minute(), now.second());
  } else {
    // Errors still show in Serial, but not on OLED (per user request)
    Serial.print("[No RTC] ");
  }

  y = 25;
  uint32_t totalSecs = nowMs / 1000;
  int h = totalSecs / 3600;
  int m = (totalSecs % 3600) / 60;
  int s = totalSecs % 60;

  snprintf(upStr, sizeof(upStr), "on: %02d:%02d:%02d", h, m, s);
  oled.drawStr(0, y, upStr);
  Serial.printf("Uptime: %s | ", upStr);

  if (!showScreen2) {
    // SCREEN 1: SHT40 (INT) & BME280 (EXT)
    y = 38;
    if (shtFound) {
      char l[32];
      snprintf(l, sizeof(l), "sht40: %.1fC  %.1f%%", shtTemp, shtHum);
      oled.drawStr(0, y, l);
      y += 13;
    }
    Serial.printf("SHT: %.1fC %.1f%% | ", shtTemp, shtHum);

    if (dhtFound) {
      char l[32];
      snprintf(l, sizeof(l), "DHT22: %.1fC  %.1f%%", dhtTemp, dhtHum);
      oled.drawStr(0, y, l);
      y += 13;
    }
    Serial.printf("DHT22: %.1fC %.1f%% | ", dhtTemp, dhtHum);

    if (sdFound) {
      oled.drawStr(0, y, "SD LOG: OK (1s)");
    } else {
      oled.drawStr(0, y, "SD LOG: FAIL");
    }
    Serial.printf("SD: %s\n", sdFound ? "OK" : "ERR");
  } else {
    // SCREEN 2: ADS/HMP60, DHT22, SD Status
    y = 38;
    if (adsFound) {
      char l[32];
      snprintf(l, sizeof(l), "HMP60: %.1fC  %.1f%%", hmpTemp, hmpHum);
      oled.drawStr(0, y, l);
      y += 13;
    }
    Serial.printf("HMP60: %.1fC %.1f%% | ", hmpTemp, hmpHum);
    if (bmeFound) {
      char l[32];
      snprintf(l, sizeof(l), "BME280: %.1fC  %.1f%%", bmeTemp, bmeHum);
      oled.drawStr(0, y, l);
      y += 13;
    }
    Serial.printf("BME280: %.1fC %.1f%% | ", bmeTemp, bmeHum);
  }

  oled.sendBuffer();
}

/* ===================== SD LOGGING ===================== */
static void logToSD() {
  if (!sdFound || !rtcFound)
    return;

  DateTime now = rtc.now();

  int ymd = (now.year() * 10000) + (now.month() * 100) + now.day();

  // Daily rotation
  if (ymd != lastYMD) {
    char fname[20];
    snprintf(fname, sizeof(fname), "/%04d%02d%02d.csv", now.year(), now.month(),
             now.day());
    currentFileName = String(fname);
    lastYMD = ymd;

    if (!SD.exists(currentFileName)) {
      File f = SD.open(currentFileName, FILE_WRITE);
      if (f) {
        f.println("Timestamp,Timeon,SHT_Temp,SHT_Hum,HMP_Temp,HMP_Hum,RefBME_"
                  "Temp,RefBME_"
                  "RefHum,BME_Press,DHT_Temp,DHT_Hum");
        f.close();
        Serial.println("[SD] New file created: " + currentFileName);
      } else {
        Serial.println("[ERR] SD header write failed");
      }
    }
  }

  File dataFile = SD.open(currentFileName, FILE_APPEND);
  if (dataFile) {
    char ts[32];
    snprintf(ts, sizeof(ts), "%04d-%02d-%02d %02d:%02d:%02d", now.year(),
             now.month(), now.day(), now.hour(), now.minute(), now.second());

    dataFile.print(ts);
    dataFile.print(",");
    dataFile.print(upStr);
    dataFile.print(",");
    dataFile.print(shtFound ? shtTemp : NAN, 1);
    dataFile.print(",");
    dataFile.print(shtFound ? shtHum : NAN, 1);
    dataFile.print(",");

    dataFile.print(adsFound ? hmpTemp : NAN, 1);
    dataFile.print(",");
    dataFile.print(adsFound ? hmpHum : NAN, 1);
    dataFile.print(",");

    dataFile.print(bmeFound ? bmeTemp : NAN, 1);
    dataFile.print(",");
    dataFile.print(bmeFound ? bmeHum : NAN, 1);
    dataFile.print(",");
    dataFile.print(bmeFound ? bmePress : NAN, 1);
    dataFile.print(",");

    dataFile.print(dhtFound ? dhtTemp : NAN, 1);
    dataFile.print(",");
    dataFile.println(dhtFound ? dhtHum : NAN, 1);

    dataFile.close();
    Serial.println("[LOG] Data written: " + currentFileName);
  } else {
    Serial.println("[ERR] SD open failed for append");
  }
}

/* ===================== SETUP / LOOP ===================== */
void setup() {
  Serial.begin(115200);
  // delay(1000);
  Serial.println("\n=== ESP32 Multi-Sensor Logger ===");

  // First init attempt
  tryInitNonBlocking();

  // Simple boot screen if OLED is ready
  if (oledFound) {
    oled.clearBuffer();
    int yStatus = 15;
    if (shtFound) {
      oled.setCursor(0, yStatus);
      oled.print("SHT: OK");
      yStatus += 15;
    }
    if (adsFound) {
      oled.setCursor(0, yStatus);
      oled.print("ADS: OK");
      yStatus += 15;
    }
    if (bmeFound) {
      oled.setCursor(0, yStatus);
      oled.print("BME: OK");
      yStatus += 15;
    }
    if (dhtFound) {
      oled.setCursor(0, yStatus);
      oled.print("DHT: OK");
      yStatus += 15;
    }
    if (rtcFound) {
      oled.setCursor(0, yStatus);
      oled.print("RTC: OK");
      yStatus += 15;
    }

    // Always show SD status at boot, even if it failed
    oled.setCursor(0, yStatus);
    oled.print(sdFound ? "SD: OK" : "SD: FAIL");
    yStatus += 15;

    if (yStatus == 15) { // nothing found
      oled.setCursor(0, 15);
      oled.print("WAITING SENSORS..");
    }
    oled.sendBuffer();
    delay(1500);
  }
}

void loop() {
  uint32_t nowMs = millis();

  // Retry init if something is missing (never abort)
  if (!oledFound || !shtFound || !adsFound || !rtcFound || !sdFound) {
    tryInitNonBlocking();
  }

  // Periodic read
  if (nowMs - lastReadTime >= READ_INTERVAL_MS) {
    lastReadTime = nowMs;
    updateSensors();
  }

  // Display update
  if (nowMs - lastDisplayTime >= DISPLAY_INTERVAL_MS) {
    lastDisplayTime = nowMs;
    updateOLED();
  }

  // SD logging
  if (nowMs - lastLogTime >= LOG_INTERVAL_MS) {
    lastLogTime = nowMs;
    logToSD();
  }

  delay(2);
}
