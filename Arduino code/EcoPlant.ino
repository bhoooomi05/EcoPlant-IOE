#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>
#include <DHT.h>
#include <time.h>

// Credentials are stored locally in secrets.h.
// Copy secrets.example.h to secrets.h and fill in your own values.
// Do not commit secrets.h to GitHub.
#include "secrets.h"

FirebaseData   fbdo;
FirebaseAuth   auth;
FirebaseConfig config;

bool firebaseReady = false;   // becomes true once Firebase.begin() has run

// ==========================
// REMOTE PUMP CONTROL
// ==========================
// AUTO   = use the existing local sensor-based irrigation logic
// MANUAL = follow the pump command coming from the website
String controlMode = "AUTO";
String remotePumpCommand = "OFF";

// ==========================
// PIN DEFINITIONS  (unchanged)
// ==========================
#define DHT_PIN D2
#define DHT_TYPE DHT11

#define SOIL_PIN A0

#define WATER_LEVEL_PIN D7

#define RELAY_PIN D1

DHT dht(DHT_PIN, DHT_TYPE);

// ==========================
// RELAY SETTINGS  (unchanged)
// ==========================
// Most relay modules are Active LOW
#define RELAY_ON  LOW
#define RELAY_OFF HIGH

// ==========================
// THRESHOLDS  (unchanged)
// ==========================
#define LOW_SOIL_MOISTURE 35     // Soil below 35% = Dry
#define HIGH_TEMPERATURE 32      // Temperature above 32°C = High
#define LOW_HUMIDITY 45          // Humidity below 45% = Low

// ==========================
// FIREBASE / WI-FI TIMING
// ==========================
unsigned long lastWifiRetryMs   = 0;
const unsigned long WIFI_RETRY_INTERVAL_MS = 10000; // try to reconnect every 10s

unsigned int  loopCounter = 0;
const unsigned int HISTORY_PUSH_EVERY_N_LOOPS = 2; // push history roughly every 2 loops (~6s)

// ==========================
// WI-FI CONNECT (blocking only at boot, with timeout)
// ==========================
void connectWiFi() {
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 15000) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Wi-Fi connected. IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Wi-Fi FAILED to connect within timeout. Will keep retrying in background.");
    Serial.println("Pump logic will continue to run locally regardless of Wi-Fi state.");
  }
}

// Non-blocking check called every loop: reconnects Wi-Fi if it drops,
// without ever pausing/blocking the sensor + pump logic.
void maintainWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  unsigned long now = millis();
  if (now - lastWifiRetryMs >= WIFI_RETRY_INTERVAL_MS) {
    lastWifiRetryMs = now;
    Serial.println("Wi-Fi disconnected. Attempting reconnect...");
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
}

// ==========================
// TIME SYNC (needed for TLS + real timestamps)
// ==========================
void syncTime() {
  Serial.print("Syncing time via NTP");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov"); // UTC epoch

  unsigned long startAttempt = millis();
  time_t now = time(nullptr);
  // 1700000000 ~= Nov 2023, used as a sanity check that NTP actually replied
  while (now < 1700000000 && millis() - startAttempt < 15000) {
    delay(300);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println();

  if (now >= 1700000000) {
    Serial.println("Time synced successfully.");
  } else {
    Serial.println("Time sync FAILED (timestamps will fall back to device uptime).");
  }
}

// Returns a millisecond epoch timestamp. Uses real NTP time when available,
// otherwise falls back to millis() (device uptime) so the field is never empty.
unsigned long long getTimestampMs() {
  time_t now = time(nullptr);
  if (now >= 1700000000) {
    return (unsigned long long)now * 1000ULL;
  }
  return (unsigned long long)millis();
}

// ==========================
// FIREBASE SETUP
// ==========================
void setupFirebase() {
  config.database_url = DATABASE_URL;
  config.api_key = API_KEY;

  // Test Mode = no authentication token is generated. This ONLY works while your
  // Realtime Database rules allow public read/write (see the DEVELOPMENT ONLY
  // rules provided separately). Switch this off once you lock the rules down.
  config.signer.test_mode = true;

  Firebase.begin(&config, &auth);
  firebaseReady = true;

  Serial.println("Firebase initialized (Test Mode - no auth token).");
}

// ==========================
// FIREBASE UPLOAD HELPERS
// ==========================
void printFirebaseError(const char *what) {
  Serial.print("Firebase ERROR (");
  Serial.print(what);
  Serial.print("): ");
  Serial.println(fbdo.errorReason());
}

// Uploads the latest readings to /ecoplant/current
void uploadCurrentReading(float temperature, float humidity, int soilMoisture,
                           bool waterLow, bool pumpOn, unsigned long long tsMs) {
  FirebaseJson json;
  json.clear();
  json.set("temperature", temperature);
  json.set("humidity", humidity);
  json.set("soilMoisture", soilMoisture);
  json.set("waterLevel", waterLow ? 0 : 100); // digital sensor -> 0 (low) or 100 (ok)
  json.set("pumpStatus", pumpOn ? "ON" : "OFF");
  json.set("timestamp", (double)tsMs);

  if (Firebase.setJSON(fbdo, "/ecoplant/current", json)) {
    Serial.println("Firebase: current data uploaded.");
  } else {
    printFirebaseError("setJSON /ecoplant/current");
  }
}

// Pushes a new record into /ecoplant/history/<push id>
void pushHistoryReading(float temperature, float humidity, int soilMoisture,
                         bool waterLow, bool pumpOn, unsigned long long tsMs) {
  FirebaseJson json;
  json.clear();
  json.set("temperature", temperature);
  json.set("humidity", humidity);
  json.set("soilMoisture", soilMoisture);
  json.set("waterLevel", waterLow ? 0 : 100);
  json.set("pumpStatus", pumpOn ? "ON" : "OFF");
  json.set("timestamp", (double)tsMs);

  if (Firebase.pushJSON(fbdo, "/ecoplant/history", json)) {
    Serial.print("Firebase: history record pushed, key = ");
    Serial.println(fbdo.pushName());
  } else {
    printFirebaseError("pushJSON /ecoplant/history");
  }
}

// ==========================
// REMOTE CONTROL READ
// ==========================
// Reads the website commands from:
// /ecoplant/control/mode  -> "AUTO" or "MANUAL"
// /ecoplant/control/pump  -> "ON" or "OFF"
//
// If Firebase is unavailable, the ESP8266 keeps the last known mode/command.
// On startup the safe defaults are AUTO + OFF.
void readRemoteControl() {
  if (WiFi.status() != WL_CONNECTED || !firebaseReady) return;

  if (Firebase.getString(fbdo, "/ecoplant/control/mode")) {
    String mode = fbdo.stringData();
    mode.trim();
    mode.toUpperCase();

    if (mode == "AUTO" || mode == "MANUAL") {
      controlMode = mode;
    }
  } else {
    printFirebaseError("getString /ecoplant/control/mode");
  }

  if (Firebase.getString(fbdo, "/ecoplant/control/pump")) {
    String command = fbdo.stringData();
    command.trim();
    command.toUpperCase();

    if (command == "ON" || command == "OFF") {
      remotePumpCommand = command;
    }
  } else {
    printFirebaseError("getString /ecoplant/control/pump");
  }

  Serial.print("Control Mode : ");
  Serial.println(controlMode);

  Serial.print("Remote Pump  : ");
  Serial.println(remotePumpCommand);
}

// ==========================
// SETUP
// ==========================
void setup() {

  Serial.begin(115200);

  dht.begin();

  pinMode(WATER_LEVEL_PIN, INPUT);

  pinMode(RELAY_PIN, OUTPUT);

  // Keep pump OFF when ESP8266 starts
  digitalWrite(RELAY_PIN, RELAY_OFF);

  delay(2000);

  Serial.println();
  Serial.println("=================================");
  Serial.println("      SMART IRRIGATION SYSTEM");
  Serial.println("           (EcoPlant)");
  Serial.println("=================================");
  Serial.println("System Started");
  Serial.println("Remote control defaults: AUTO + OFF");
  Serial.println();

  // ---- Wi-Fi + Firebase bring-up ----
  // NOTE: even if any of this fails, the pump/sensor logic below still runs.
  connectWiFi();

  if (WiFi.status() == WL_CONNECTED) {
    syncTime();
    setupFirebase();
  } else {
    Serial.println("Skipping time sync / Firebase setup until Wi-Fi connects.");
  }

  Serial.println();
}

// ==========================
// MAIN LOOP
// ==========================
void loop() {

  // --------------------------
  // WI-FI / FIREBASE HOUSEKEEPING (non-blocking)
  // --------------------------
  maintainWiFi();

  if (WiFi.status() == WL_CONNECTED && !firebaseReady) {
    // Wi-Fi just (re)connected for the first time -> bring Firebase up now
    syncTime();
    setupFirebase();
  }

  // --------------------------
  // READ DHT11
  // --------------------------

  float temperature = dht.readTemperature();

  float humidity = dht.readHumidity();

  // Check DHT11 reading

  if (isnan(temperature) || isnan(humidity)) {

    Serial.println("ERROR: DHT11 reading failed!");

    // Safety: turn pump OFF

    digitalWrite(RELAY_PIN, RELAY_OFF);

    delay(3000);

    return;

  }

  // --------------------------
  // READ SOIL MOISTURE
  // --------------------------

  int soilRaw = analogRead(SOIL_PIN);

  /*

     ESP8266 ADC:

     0   = very wet

     1023 = very dry

     Convert to percentage:

     1023 -> 0%

     0    -> 100%

  */

  int soilMoisture = map(soilRaw, 1023, 0, 0, 100);

  soilMoisture = constrain(soilMoisture, 0, 100);

  // --------------------------
  // READ WATER LEVEL
  // --------------------------

  int waterSensor = digitalRead(WATER_LEVEL_PIN);

  /*

     Current assumption based on your sensor testing:

     HIGH (1) = Water available

     LOW  (0) = Water level low

  */

  bool waterLow = (waterSensor == LOW);

  // --------------------------
  // DISPLAY SENSOR VALUES
  // --------------------------

  Serial.println("---------------------------------");

  Serial.print("Wi-Fi Status : ");
  Serial.println(WiFi.status() == WL_CONNECTED ? "CONNECTED" : "DISCONNECTED");

  Serial.print("Firebase     : ");
  Serial.println(firebaseReady ? "READY (test mode)" : "NOT INITIALIZED");

  Serial.print("Temperature : ");

  Serial.print(temperature);

  Serial.println(" °C");

  Serial.print("Humidity    : ");

  Serial.print(humidity);

  Serial.println(" %");

  Serial.print("Soil Raw    : ");

  Serial.println(soilRaw);

  Serial.print("Soil Moist. : ");

  Serial.print(soilMoisture);

  Serial.println(" %");

  Serial.print("Water Sensor: ");

  Serial.println(waterSensor);

  // --------------------------
  // CHECK CONDITIONS
  // --------------------------

  bool soilDry = (soilMoisture < LOW_SOIL_MOISTURE);

  bool temperatureHigh = (temperature > HIGH_TEMPERATURE);

  bool humidityLow = (humidity < LOW_HUMIDITY);

  Serial.println();

  Serial.print("Soil Dry?         : ");

  Serial.println(soilDry ? "YES" : "NO");

  Serial.print("Temperature High? : ");

  Serial.println(temperatureHigh ? "YES" : "NO");

  Serial.print("Humidity Low?     : ");

  Serial.println(humidityLow ? "YES" : "NO");

  Serial.print("Water Available?  : ");

  Serial.println(waterLow ? "NO" : "YES");

  // --------------------------
  // READ REMOTE CONTROL COMMAND
  // --------------------------
  // This is only a command source. The existing local safety logic below
  // remains the final authority over the relay.
  readRemoteControl();

  // --------------------------
  // PUMP CONTROL
  // --------------------------
  /*

     Pump turns ON when:

     1. Soil is dry

        OR

     2. Temperature is high

        OR

     3. Humidity is low

     BUT

     Water level MUST be sufficient.

     This logic runs locally on the ESP8266 and NEVER depends on
     Wi-Fi or Firebase being available.

  */

  bool pumpOn = false;

  // SAFETY #1: water-low always overrides everything.
  if (waterLow) {

    digitalWrite(RELAY_PIN, RELAY_OFF);
    pumpOn = false;

    Serial.println();
    Serial.println("WARNING: WATER LEVEL LOW!");
    Serial.println("PUMP: OFF (SAFETY OVERRIDE)");

  }

  // MANUAL mode: follow the website command.
  else if (controlMode == "MANUAL") {

    if (remotePumpCommand == "ON") {
      digitalWrite(RELAY_PIN, RELAY_ON);
      pumpOn = true;

      Serial.println();
      Serial.println("MANUAL CONTROL");
      Serial.println("WEBSITE COMMAND: ON");
      Serial.println("PUMP: ON");
    }

    else {
      digitalWrite(RELAY_PIN, RELAY_OFF);
      pumpOn = false;

      Serial.println();
      Serial.println("MANUAL CONTROL");
      Serial.println("WEBSITE COMMAND: OFF");
      Serial.println("PUMP: OFF");
    }

  }

  // AUTO mode: your original sensor-based logic.
  else {

    if (soilDry || temperatureHigh || humidityLow) {

      digitalWrite(RELAY_PIN, RELAY_ON);
      pumpOn = true;

      Serial.println();
      Serial.println("AUTO IRRIGATION REQUIRED");
      Serial.println("PUMP: ON");

    }

    else {

      digitalWrite(RELAY_PIN, RELAY_OFF);
      pumpOn = false;

      Serial.println();
      Serial.println("AUTO CONDITIONS NORMAL");
      Serial.println("PUMP: OFF");
    }
  }

  // --------------------------
  // FIREBASE UPLOAD (best-effort; never blocks pump safety logic above)
  // --------------------------
  if (WiFi.status() == WL_CONNECTED && firebaseReady) {

    unsigned long long tsMs = getTimestampMs();

    uploadCurrentReading(temperature, humidity, soilMoisture, waterLow, pumpOn, tsMs);

    loopCounter++;
    if (loopCounter >= HISTORY_PUSH_EVERY_N_LOOPS) {
      loopCounter = 0;
      pushHistoryReading(temperature, humidity, soilMoisture, waterLow, pumpOn, tsMs);
    }

  } else {
    Serial.println("Firebase upload skipped (Wi-Fi/Firebase not ready). Pump logic still active.");
  }

  Serial.println("---------------------------------");

  Serial.println();

  // Wait 3 seconds before next reading

  delay(3000);

}
