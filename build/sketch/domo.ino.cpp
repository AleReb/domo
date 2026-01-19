#line 1 "C:\\Users\\Ale\\Documents\\Arduino\\domo\\domo.ino"
#include <Adafruit_ADS1X15.h>
#include <Arduino.h>
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

/* ===== I2C CONFIG ===== */
static const uint8_t OLED_ADDR_7BIT = 0x3C;
static const uint8_t ADS_ADDR_7BIT = 0x48;

/* ===== CONSTANTS ===== */
static const uint32_t LOG_INTERVAL_MS = 60000;    // 1 minute
static const uint32_t DISPLAY_INTERVAL_MS = 1000; // 1 second
static const uint32_t READ_INTERVAL_MS = 2000;    // 2 seconds
static const uint32_t INIT_RETRY_MS = 3000;       // retry init every 3 seconds

/* ===== GLOBAL OBJECTS ===== */
// OLED (128x64 SSD1306)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

// SHT40
SHT4x sht(0x44, &Wire);

// ADS1115
Adafruit_ADS1115 ads;

// RTC
RTC_DS3231 rtc;

// SD (HSPI)
SPIClass sdSPI(HSPI);

/* ===== STATUS FLAGS ===== */
static bool oledFound = false;
static bool shtFound = false;
static bool adsFound = false;
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

/* ===================== HELPERS ===================== */
#line 66 "C:\\Users\\Ale\\Documents\\Arduino\\domo\\domo.ino"
static float clampf(float x, float lo, float hi);
#line 75 "C:\\Users\\Ale\\Documents\\Arduino\\domo\\domo.ino"
static void tryInitNonBlocking();
#line 148 "C:\\Users\\Ale\\Documents\\Arduino\\domo\\domo.ino"
static void updateSensors();
#line 185 "C:\\Users\\Ale\\Documents\\Arduino\\domo\\domo.ino"
static void updateOLED();
#line 241 "C:\\Users\\Ale\\Documents\\Arduino\\domo\\domo.ino"
static void logToSD();
#line 295 "C:\\Users\\Ale\\Documents\\Arduino\\domo\\domo.ino"
void setup();
#line 319 "C:\\Users\\Ale\\Documents\\Arduino\\domo\\domo.ino"
void loop();
#line 66 "C:\\Users\\Ale\\Documents\\Arduino\\domo\\domo.ino"
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
}

/* ===================== OLED UPDATE ===================== */
static void updateOLED() {
  if (!oledFound)
    return;

  oled.clearBuffer();
  oled.setFont(u8g2_font_6x10_tf);

  // Time header
  if (rtcFound) {
    DateTime now = rtc.now();
    char ts[32];
    snprintf(ts, sizeof(ts), "%02d/%02d/%04d %02d:%02d:%02d", now.day(),
             now.month(), now.year(), now.hour(), now.minute(), now.second());
    oled.drawStr(0, 12, ts);

    Serial.printf("[%02d:%02d:%02d] ", now.hour(), now.minute(), now.second());
  } else {
    oled.drawStr(0, 12, "RTC ERROR");
    Serial.print("[No RTC] ");
  }

  // SHT40
  if (shtFound) {
    char l1[32];
    snprintf(l1, sizeof(l1), "SHT40: %.1fC  %.1f%%", shtTemp, shtHum);
    oled.drawStr(0, 30, l1);
    Serial.printf("SHT: %.1fC %.1f%% | ", shtTemp, shtHum);
  } else {
    oled.drawStr(0, 30, "SHT40: ERROR");
    Serial.print("SHT: ERR | ");
  }

  // HMP60 (ADS1115)
  if (adsFound) {
    char l2[32];
    snprintf(l2, sizeof(l2), "HMP60: %.1fC  %.1f%%", hmpTemp, hmpHum);
    oled.drawStr(0, 48, l2);
    Serial.printf("HMP: %.1fC %.1f%% | ", hmpTemp, hmpHum);
  } else {
    oled.drawStr(0, 48, "HMP60: ERROR");
    Serial.print("HMP: ERR | ");
  }

  // SD status
  if (sdFound) {
    oled.drawStr(0, 64, "SD LOG: OK (1m)");
    Serial.println("SD: OK");
  } else {
    oled.drawStr(0, 64, "SD LOG: FAILED");
    Serial.println("SD: ERR");
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
        f.println("Timestamp,SHT_Temp,SHT_Hum,HMP_Temp,HMP_Hum");
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

    dataFile.print(shtFound ? shtTemp : NAN, 1);
    dataFile.print(",");
    dataFile.print(shtFound ? shtHum : NAN, 1);
    dataFile.print(",");

    dataFile.print(adsFound ? hmpTemp : NAN, 1);
    dataFile.print(",");
    dataFile.println(adsFound ? hmpHum : NAN, 1);

    dataFile.close();
    Serial.println("[LOG] Data written: " + currentFileName);
  } else {
    Serial.println("[ERR] SD open failed for append");
  }
}

/* ===================== SETUP / LOOP ===================== */
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== ESP32 Multi-Sensor Logger ===");

  // First init attempt
  tryInitNonBlocking();

  // Simple boot screen if OLED is ready
  if (oledFound) {
    oled.clearBuffer();
    oled.setCursor(0, 15);
    oled.print(shtFound ? "SHT: OK" : "SHT: FAIL");
    oled.setCursor(0, 30);
    oled.print(adsFound ? "ADS: OK" : "ADS: FAIL");
    oled.setCursor(0, 45);
    oled.print(rtcFound ? "RTC: OK" : "RTC: FAIL");
    oled.setCursor(0, 60);
    oled.print(sdFound ? "SD: OK" : "SD: FAIL");
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

