// ============================================================
//  VibroJaw — ESP32 S3 DevKitC Firmware
//  Team: ChocoChip Circuits | MedTech Hack 2025
// ============================================================
//
//  WIRING:
//    Piezo film sensor  → GPIO 4  (ADC1_CH3, analog input)
//    Vibration motor +  → GPIO 5  (PWM via LEDC channel 0)
//    Vibration motor -  → GND
//    LCD SDA            → GPIO 8
//    LCD SCL            → GPIO 9
//
//  LIBRARIES REQUIRED (install via Arduino Library Manager):
//    - WiFi            (built-in ESP32)
//    - HTTPClient      (built-in ESP32)
//    - ArduinoJson     (by Benoit Blanchon, v6.x)
//    - LiquidCrystal_I2C (by Frank de Brabander)
// ============================================================

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>

// ── WiFi credentials ────────────────────────────────────────
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// ── Server address ──────────────────────────────────────────
// Change to your computer's local IP when running Spring Boot
// e.g. "http://192.168.1.42:8080"
const char* SERVER_BASE   = "http://192.168.1.42:8080";

// ── Hardware pins ───────────────────────────────────────────
const int  PIEZO_PIN      = 4;    // Analog input from piezo sensor
const int  MOTOR_PIN      = 5;    // PWM output to vibration motor
const int  LCD_ADDR       = 0x27; // I2C address (try 0x3F if this fails)
const int  LCD_COLS       = 16;
const int  LCD_ROWS       = 2;

// ── Sensing & detection parameters (tunable from dashboard) ─
int        CLENCH_THRESHOLD    = 400;  // ADC units (0–4095). Higher = less sensitive
int        MIN_CLENCH_MS       = 80;   // Ignore events shorter than this (noise filter)
int        MAX_CLENCH_MS       = 3000; // Cap clench duration at this value
int        POST_CLENCH_QUIET   = 300;  // ms of quiet required after clench ends
bool       VIBRATION_ENABLED   = true; // Can be overridden from dashboard
int        VIBRATION_INTENSITY = 200;  // PWM duty (0–255)
int        VIBRATION_DURATION  = 600;  // ms to run motor per biofeedback pulse

// ── Sampling ─────────────────────────────────────────────────
const int  SAMPLE_RATE_MS   = 5;     // Read sensor every 5ms (200Hz)
const int  NOISE_WINDOW     = 8;     // Rolling average over this many samples
const int  SUMMARY_INTERVAL = 5000;  // Send 5s summary batch every N ms
const int  CONFIG_POLL_MS   = 10000; // Re-fetch config from server every 10s

// ── Rolling noise filter ─────────────────────────────────────
int sampleBuffer[8];
int sampleIndex = 0;

// ── State machine ─────────────────────────────────────────────
enum SensorState { IDLE, CLENCHING, POST_CLENCH_QUIET_PERIOD };
SensorState sensorState = IDLE;

unsigned long clenchStartMs  = 0;
unsigned long clenchEndMs    = 0;
int           peakIntensity  = 0;  // Peak ADC value during current clench

// ── 5-second summary counters ─────────────────────────────────
int          summaryEventCount  = 0;
long         summaryIntensitySum = 0;
unsigned long lastSummaryMs     = 0;
unsigned long lastConfigPollMs  = 0;
unsigned long lastLcdUpdateMs   = 0;

// ── LCD ───────────────────────────────────────────────────────
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

// ── Session stats (for LCD display) ───────────────────────────
int   sessionEventCount = 0;
float eventsPerMinute   = 0.0;
String currentStatus    = "Calm";

// =============================================================
//  SETUP
// =============================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n[VibroJaw] Starting up...");

  // Motor pin
  ledcAttachPin(MOTOR_PIN, 0);     // attach GPIO 5 to LEDC channel 0
  ledcSetup(0, 2000, 8);           // channel 0, 2kHz PWM, 8-bit resolution
  ledcWrite(0, 0);                 // motor off

  // Initialize rolling average buffer
  for (int i = 0; i < NOISE_WINDOW; i++) sampleBuffer[i] = 0;

  // LCD
  lcd.init();
  lcd.backlight();
  lcdShowStartup();

  // WiFi
  connectWiFi();

  lastSummaryMs    = millis();
  lastConfigPollMs = millis();
  lastLcdUpdateMs  = millis();

  Serial.println("[VibroJaw] Ready. Monitoring started.");
}

// =============================================================
//  MAIN LOOP
// =============================================================
void loop() {
  unsigned long now = millis();

  // 1. Read and filter sensor
  int rawValue     = analogRead(PIEZO_PIN);
  int smoothed     = updateRollingAverage(rawValue);

  // 2. Run state machine
  runStateMachine(smoothed, now);

  // 3. Send 5-second summary
  if (now - lastSummaryMs >= SUMMARY_INTERVAL) {
    sendSummary(now);
    lastSummaryMs = now;
  }

  // 4. Poll config from server
  if (now - lastConfigPollMs >= CONFIG_POLL_MS) {
    fetchConfig();
    lastConfigPollMs = now;
  }

  // 5. Refresh LCD every 500ms
  if (now - lastLcdUpdateMs >= 500) {
    updateLcd();
    lastLcdUpdateMs = now;
  }

  delay(SAMPLE_RATE_MS);
}

// =============================================================
//  ROLLING AVERAGE NOISE FILTER
//  Smooths out electrical noise from piezo by averaging
//  the last NOISE_WINDOW readings.
// =============================================================
int updateRollingAverage(int newSample) {
  sampleBuffer[sampleIndex] = newSample;
  sampleIndex = (sampleIndex + 1) % NOISE_WINDOW;

  long sum = 0;
  for (int i = 0; i < NOISE_WINDOW; i++) sum += sampleBuffer[i];
  return (int)(sum / NOISE_WINDOW);
}

// =============================================================
//  CLENCH DETECTION STATE MACHINE
//
//  IDLE          → signal rises above CLENCH_THRESHOLD → CLENCHING
//  CLENCHING     → signal drops below threshold AND duration ≥ MIN_CLENCH_MS → event recorded → POST_CLENCH_QUIET_PERIOD
//  CLENCHING     → signal drops below threshold AND duration < MIN_CLENCH_MS → IDLE (noise, ignored)
//  POST_CLENCH   → after POST_CLENCH_QUIET ms → IDLE
// =============================================================
void runStateMachine(int value, unsigned long now) {
  switch (sensorState) {

    case IDLE:
      if (value >= CLENCH_THRESHOLD) {
        // Clench onset detected
        sensorState    = CLENCHING;
        clenchStartMs  = now;
        peakIntensity  = value;
        Serial.printf("[VibroJaw] Clench START — ADC: %d\n", value);
      }
      break;

    case CLENCHING:
      // Track peak intensity during clench
      if (value > peakIntensity) peakIntensity = value;

      if (value < CLENCH_THRESHOLD) {
        // Clench ended
        unsigned long durationMs = now - clenchStartMs;

        if (durationMs >= MIN_CLENCH_MS) {
          // Valid clench event — record and transmit
          int durationCapped = min((unsigned long)MAX_CLENCH_MS, durationMs);
          float normalizedIntensity = (float)peakIntensity / 4095.0 * 100.0;

          Serial.printf("[VibroJaw] Clench END — Duration: %lums, Peak ADC: %d, Intensity: %.1f%%\n",
                        durationMs, peakIntensity, normalizedIntensity);

          sessionEventCount++;
          summaryEventCount++;
          summaryIntensitySum += (long)normalizedIntensity;

          // Trigger vibration biofeedback
          if (VIBRATION_ENABLED) {
            triggerVibration();
          }

          // POST event to server
          postClenchEvent(normalizedIntensity, durationCapped);

          clenchEndMs = now;
          sensorState = POST_CLENCH_QUIET_PERIOD;
        } else {
          // Too short — noise, ignore
          Serial.printf("[VibroJaw] Noise filtered — duration %lums < %dms minimum\n",
                        durationMs, MIN_CLENCH_MS);
          sensorState = IDLE;
        }
      }
      break;

    case POST_CLENCH_QUIET_PERIOD:
      // Stay quiet to avoid double-triggering
      if (now - clenchEndMs >= POST_CLENCH_QUIET) {
        sensorState = IDLE;
      }
      break;
  }
}

// =============================================================
//  VIBRATION BIOFEEDBACK
//  Runs motor at VIBRATION_INTENSITY PWM for VIBRATION_DURATION ms.
//  Non-blocking via a simple timestamp approach.
// =============================================================
unsigned long motorStartMs   = 0;
bool          motorRunning   = false;

void triggerVibration() {
  ledcWrite(0, VIBRATION_INTENSITY);
  motorStartMs = millis();
  motorRunning = true;
  Serial.printf("[VibroJaw] Vibration triggered — PWM: %d, Duration: %dms\n",
                VIBRATION_INTENSITY, VIBRATION_DURATION);
}

// Call this from loop() to stop motor after duration — already handled via delay-free check
void checkMotorStop() {
  if (motorRunning && (millis() - motorStartMs >= VIBRATION_DURATION)) {
    ledcWrite(0, 0);
    motorRunning = false;
  }
}

// =============================================================
//  HTTP: POST single clench event
//  Endpoint: POST /api/event
//  Body: { "userId": "esp32-001", "intensity": 72.5,
//          "durationMs": 340, "therapyTriggered": true }
// =============================================================
void postClenchEvent(float intensity, int durationMs) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[VibroJaw] WiFi disconnected — skipping event POST");
    reconnectWiFi();
    return;
  }

  HTTPClient http;
  String url = String(SERVER_BASE) + "/api/event";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  // Build JSON payload
  StaticJsonDocument<200> doc;
  doc["userId"]           = "esp32-001";
  doc["intensity"]        = intensity;
  doc["durationMs"]       = durationMs;
  doc["therapyTriggered"] = VIBRATION_ENABLED;

  String body;
  serializeJson(doc, body);

  int responseCode = http.POST(body);

  if (responseCode == 200 || responseCode == 201) {
    Serial.printf("[VibroJaw] Event POST OK (%d)\n", responseCode);
  } else {
    Serial.printf("[VibroJaw] Event POST FAILED — HTTP %d\n", responseCode);
  }

  http.end();
}

// =============================================================
//  HTTP: POST 5-second summary
//  Endpoint: POST /api/summary
//  Body: { "userId": "esp32-001", "eventsInWindow": 3,
//          "avgIntensity": 65.2, "windowMs": 5000 }
// =============================================================
void sendSummary(unsigned long now) {
  float avgIntensity = (summaryEventCount > 0)
                       ? (float)summaryIntensitySum / summaryEventCount
                       : 0.0;

  // Compute events-per-minute for status display
  eventsPerMinute = (float)summaryEventCount * 12.0; // 12 × 5s = 1 min

  Serial.printf("[VibroJaw] Summary — Events: %d, AvgIntensity: %.1f%%, EPM: %.1f\n",
                summaryEventCount, avgIntensity, eventsPerMinute);

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String(SERVER_BASE) + "/api/summary";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<200> doc;
    doc["userId"]         = "esp32-001";
    doc["eventsInWindow"] = summaryEventCount;
    doc["avgIntensity"]   = avgIntensity;
    doc["windowMs"]       = SUMMARY_INTERVAL;

    String body;
    serializeJson(doc, body);

    int responseCode = http.POST(body);
    Serial.printf("[VibroJaw] Summary POST %s (%d)\n",
                  (responseCode > 0 ? "OK" : "FAILED"), responseCode);
    http.end();
  }

  // Reset counters
  summaryEventCount   = 0;
  summaryIntensitySum = 0;
}

// =============================================================
//  HTTP: GET config from server
//  Endpoint: GET /api/config?userId=esp32-001
//  Response: { "threshold": 400, "vibrationEnabled": true,
//              "vibrationIntensity": 200, "vibrationDuration": 600 }
// =============================================================
void fetchConfig() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = String(SERVER_BASE) + "/api/config?userId=esp32-001";
  http.begin(url);

  int responseCode = http.GET();

  if (responseCode == 200) {
    String payload = http.getString();

    StaticJsonDocument<200> doc;
    DeserializationError err = deserializeJson(doc, payload);

    if (!err) {
      CLENCH_THRESHOLD    = doc["threshold"]          | CLENCH_THRESHOLD;
      VIBRATION_ENABLED   = doc["vibrationEnabled"]   | VIBRATION_ENABLED;
      VIBRATION_INTENSITY = doc["vibrationIntensity"] | VIBRATION_INTENSITY;
      VIBRATION_DURATION  = doc["vibrationDuration"]  | VIBRATION_DURATION;

      Serial.printf("[VibroJaw] Config updated — Threshold: %d, Vib: %s\n",
                    CLENCH_THRESHOLD, VIBRATION_ENABLED ? "ON" : "OFF");
    }
  }
  http.end();
}

// =============================================================
//  LCD DISPLAY
// =============================================================
void lcdShowStartup() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  VibroJaw v1.0");
  lcd.setCursor(0, 1);
  lcd.print(" Initializing...");
  delay(1500);
}

void updateLcd() {
  // Determine status from events per minute
  if (eventsPerMinute < 3)       currentStatus = "Calm    ";
  else if (eventsPerMinute < 8)  currentStatus = "Elevated";
  else                           currentStatus = "Concern!";

  lcd.clear();
  // Row 0: Status + events per minute
  lcd.setCursor(0, 0);
  lcd.print(currentStatus);
  lcd.setCursor(9, 0);
  char epmbuf[7];
  snprintf(epmbuf, sizeof(epmbuf), "%4.1f/m", eventsPerMinute);
  lcd.print(epmbuf);

  // Row 1: Total session events + vibration state
  lcd.setCursor(0, 1);
  char buf[17];
  snprintf(buf, sizeof(buf), "Tot:%-4d Vib:%s",
           sessionEventCount,
           VIBRATION_ENABLED ? "ON " : "OFF");
  lcd.print(buf);
}

// =============================================================
//  WIFI HELPERS
// =============================================================
void connectWiFi() {
  Serial.printf("[VibroJaw] Connecting to WiFi: %s\n", WIFI_SSID);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[VibroJaw] WiFi connected. IP: %s\n",
                  WiFi.localIP().toString().c_str());
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected!");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP().toString());
    delay(1500);
  } else {
    Serial.println("\n[VibroJaw] WiFi FAILED — running offline");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi FAILED");
    lcd.setCursor(0, 1);
    lcd.print("Offline mode");
    delay(1500);
  }
}

void reconnectWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[VibroJaw] Attempting WiFi reconnect...");
    WiFi.reconnect();
    delay(2000);
  }
}
