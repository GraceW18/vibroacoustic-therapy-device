

#include <WiFi.h>
#include <HTTPClient.h>

// WiFi & Server
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "WIFI_PASSWORD";
const char* SERVER   = "https://vibroacoustic-therapy-device-production.up.railway.app";
const char* USER_ID  = "esp32-001";

// Pins
const int piezoPin     = 6;   // ADC input-only (was A0 on Uno)
const int detectLEDPin = 2;    // onboard LED on most ESP32 DevKits
const int motorPin     = 25;   // same transistor circuit, different pin number

const bool SERIAL_DEBUG = true;

// Signal processing
float filtered     = 0.0;
float slowBaseline = 0.0;
float prevFiltered = 0.0;

const float alphaFast = 0.20;
const float alphaSlow = 0.01;

// Thresholds
int threshold          = 120;
int suddenSpikeDelta   = 4;
int artifactJump       = 299;
int maxDeformation     = 640;

bool VIB_ENABLED = true;  // updated by pollConfig

// Timing / pattern
unsigned long lastTriggerMs = 0;
const unsigned long refractoryMs  = 900;
unsigned long windowStartMs       = 0;
const unsigned long spikeWindowMs = 280;
int  spikeCountInWindow           = 0;
const int maxSpikesForClench      = 2;

// Motor
bool          motorActive        = false;
unsigned long motorEndMs         = 0;
unsigned long motorLastToggleMs  = 0;
bool          motorState         = false;
unsigned int  motorHalfPeriodMs  = 10;
unsigned long motorBurstDuration = 220;

// Data capture for HTTP POST
// Filled in at moment of trigger, sent after motor burst ends
float   pendingIntensity  = 0.0;
int     pendingDurationMs = 0;
bool    pendingEvent      = false;

// 5-second summary window
unsigned long summaryWindowStart = 0;
int           summaryEvents      = 0;
float         summaryIntensity   = 0.0;

// Config poll
unsigned long lastPollMs = 0;

// Signal Processing Hard Coded
int count = 0;
int rawCollection[5] = {0};

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(detectLEDPin, OUTPUT);
  pinMode(motorPin, OUTPUT);
  digitalWrite(detectLEDPin, LOW);
  digitalWrite(motorPin, LOW);

  // ESP32 ADC is 12-bit by default, but set explicitly to be safe
  analogReadResolution(12);

  calibrateBaseline();

  // Connect WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected! IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("Sending data to: ");
  Serial.println(String(SERVER) + "/api/event");

  windowStartMs        = millis();
  summaryWindowStart   = millis();
  lastPollMs           = millis();
}

void loop() {
  unsigned long now = millis();
  if (count == 5) {
    int rawSum = 0;
    for(int i = 0; i <= 4; i++){
      rawSum += rawCollection[i];
    }
    int raw = 300 - rawSum / 5;

    // Signal processing
    prevFiltered = filtered;
    filtered = alphaFast * raw + (1.0 - alphaFast) * filtered;

    // Slow baseline — only tracks when motor is off and near rest
    int nearRest = abs((int)(filtered - slowBaseline));
    if (!motorActive && nearRest < threshold) {
      slowBaseline = alphaSlow * filtered + (1.0 - alphaSlow) * slowBaseline;
    }

    int deviation = abs((int)(filtered - slowBaseline));
    int delta     = (int)(filtered - prevFiltered);
    int absDelta  = abs(delta);
    // Serial.println(absDelta);

    bool artifact       = (absDelta > artifactJump);
    // bool candidateSpike = (!artifact && deviation > threshold && absDelta >= suddenSpikeDelta);
    bool candidateSpike = (filtered > artifactJump);
    // Spike window (rejects repetitive chewing/open-close motion)
    if (now - windowStartMs > spikeWindowMs) {
      windowStartMs      = now;
      spikeCountInWindow = 0;
    }
    if (candidateSpike) spikeCountInWindow++;

    bool likelyContinuousMotion = (spikeCountInWindow > maxSpikesForClench);

    digitalWrite(detectLEDPin, (deviation > threshold) ? HIGH : LOW);

    // Trigger
    bool cooldownDone = (now - lastTriggerMs > refractoryMs);
    if (candidateSpike && cooldownDone && !likelyContinuousMotion) {
      // Capture data before starting the motor burst
      // Intensity: map deviation to 0–100%
      float intensity = ((float)(deviation - threshold) /
                        (float)(maxDeformation  - threshold)) * 100.0f;
      if (intensity < 0)   intensity = 0;
      if (intensity > 100) intensity = 100;

      startMotorBurst(deviation);   // sets motorBurstDuration for this hit

      // Store for HTTP POST (sent after burst so POST doesn't block motor timing)
      pendingIntensity  = intensity;
      pendingDurationMs = (int)motorBurstDuration;
      pendingEvent      = true;

      lastTriggerMs = now;

      if (SERIAL_DEBUG) {
        Serial.printf("[CLENCH] deviation=%d intensity=%.1f%% duration=%lums\n",
                      deviation, intensity, motorBurstDuration);
      }
    }

    updateMotor(now);

    // Send pending event AFTER motor burst finishes
    // This avoids the HTTP POST blocking the motor toggle loop.
    if (pendingEvent && !motorActive) {
      postEvent(pendingIntensity, pendingDurationMs, true);
      summaryEvents++;
      summaryIntensity += pendingIntensity;
      pendingEvent = false;
    }

    // 5-second summary POST
    if (now - summaryWindowStart >= 5000) {
      float avgI = summaryEvents > 0 ? (summaryIntensity / summaryEvents) : 0.0f;
      postSummary(summaryEvents, avgI, 5000);

      summaryEvents    = 0;
      summaryIntensity = 0.0;
      summaryWindowStart = now;
    }

    // Config poll every 10s
    if (now - lastPollMs >= 10000) {
      pollConfig();
      lastPollMs = now;
    }

    // Serial debug
    if (SERIAL_DEBUG) {
      static unsigned long lastPrint = 0;
      if (now - lastPrint > 50) {
        lastPrint = now;
        Serial.printf("raw=%d filt=%d base=%d dev=%d d=%d spikes=%d motor=%d\n",
                      raw, (int)filtered, (int)slowBaseline,
                      deviation, delta, spikeCountInWindow, motorActive ? 1 : 0);
      }
    }
    delay(5);
    count = 0;
  } else {
    rawCollection[count] = analogRead(piezoPin);
    count += 1;
    delay(5);
  }
}

// Motor functions

void startMotorBurst(int deviation) {
  int d = deviation;
  if (d < threshold)     d = threshold;
  if (d > maxDeformation) d = maxDeformation;

  int freqHz = map(d, threshold, maxDeformation, 35, 110);
  if (freqHz < 1) freqHz = 1;

  motorHalfPeriodMs  = 500UL / (unsigned long)freqHz;
  if (motorHalfPeriodMs < 1) motorHalfPeriodMs = 1;

  motorBurstDuration = map(d, threshold, maxDeformation, 140, 320);

  motorActive       = true;
  motorEndMs        = millis() + motorBurstDuration;
  motorLastToggleMs = 0;
  motorState        = false;
  digitalWrite(motorPin, LOW);
}

void updateMotor(unsigned long now) {
  if (!motorActive) return;

  if (now >= motorEndMs) {
    motorActive = false;
    motorState  = false;
    digitalWrite(motorPin, LOW);
    return;
  }

  if (motorLastToggleMs == 0 || (now - motorLastToggleMs >= motorHalfPeriodMs)) {
    motorLastToggleMs = now;
    motorState        = !motorState;
    digitalWrite(motorPin, motorState ? HIGH : LOW);
  }
}

//  Calibration
void calibrateBaseline() {
  long sum = 0;
  const int samples = 250;

  if (SERIAL_DEBUG) Serial.println("Calibrating baseline — keep jaw relaxed...");

  for (int i = 0; i < samples; i++) {
    sum += analogRead(piezoPin);
    delay(4);
  }

  float baseline = (float)(sum / samples);
  filtered       = baseline;
  prevFiltered   = baseline;
  slowBaseline   = baseline;

  if (SERIAL_DEBUG) {
    Serial.printf("Baseline = %.1f (out of 4095)\n", baseline);
  }
}

//  HTTP functions
void postEvent(float intensity, int durationMs, bool therapy) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(String(SERVER) + "/api/event");
  http.addHeader("Content-Type", "application/json");

  String body = String("{")
    + "\"userId\":\""         + USER_ID              + "\","
    + "\"intensity\":"        + String(intensity, 1)  + ","
    + "\"durationMs\":"       + String(durationMs)    + ","
    + "\"therapyTriggered\":" + (therapy ? "true" : "false")
    + "}";

  int code = http.POST(body);
  if (SERIAL_DEBUG) Serial.printf("[HTTP] POST /api/event → %d\n", code);
  http.end();
}

void postSummary(int events, float avgIntensity, int windowMs) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(String(SERVER) + "/api/summary");
  http.addHeader("Content-Type", "application/json");

  String body = String("{")
    + "\"userId\":\""       + USER_ID                 + "\","
    + "\"eventsInWindow\":" + String(events)           + ","
    + "\"avgIntensity\":"   + String(avgIntensity, 1)  + ","
    + "\"windowMs\":"       + String(windowMs)
    + "}";

  int code = http.POST(body);
  if (SERIAL_DEBUG) Serial.printf("[HTTP] POST /api/summary → %d (events=%d avg=%.1f%%)\n",
                                   code, events, avgIntensity);
  http.end();
}

void pollConfig() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(String(SERVER) + "/api/config?userId=" + String(USER_ID));
  int code = http.GET();

  if (code == 200) {
    String p = http.getString();
    // Pull threshold back and un-scale to ESP32 ADC range (stored as Uno values in Flask)
    int t = extractInt(p, "threshold", threshold);
    // If the value coming back looks like Uno range (< 500), scale it up
    threshold       = (t < 500) ? t * 4 : t;
    VIB_ENABLED     = extractBool(p, "vibrationEnabled", true);
    if (SERIAL_DEBUG) Serial.printf("[CONFIG] threshold=%d\n", threshold);
  }
  http.end();
}

int extractInt(String json, String key, int fallback) {
  String search = "\"" + key + "\":";
  int i = json.indexOf(search);
  if (i == -1) return fallback;
  int start = i + search.length();
  int end   = json.indexOf(",", start);
  if (end == -1) end = json.indexOf("}", start);
  return (end == -1) ? fallback : json.substring(start, end).toInt();
}

bool extractBool(String json, String key, bool fallback) {
  String search = "\"" + key + "\":";
  int i = json.indexOf(search);
  if (i == -1) return fallback;
  return json.substring(i + search.length(), i + search.length() + 4) == "true";
}