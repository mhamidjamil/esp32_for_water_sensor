/*
  water_sensor.ino
  ESP32 sketch to detect water-touch sensor and call a configurable webhook URL.

  Behavior:
  - Reads a digital water sensor pin (configurable)
  - Debounces input
  - Sends an HTTP POST to configured webhook URL when water is detected
  - Rate-limits repeated notifications while sensor stays wet (default 5 minutes)
  - Simple WiFi reconnect logic and serial logging

  Configure WiFi credentials and WEBHOOK_URL below before uploading.

  Notes:
  - Many water sensors are active LOW or active HIGH depending on wiring. Set
    SENSOR_ACTIVE_STATE accordingly.
  - Built for Arduino core for ESP32 (use recent esp32 board package).
*/

#include <WiFi.h>
#include <HTTPClient.h>


// ====== USER CONFIG ======
// Secrets are now stored in secrets.h (not tracked by git)
#include "secrets.h"

// WIFI_SSID, WIFI_PASS, WEBHOOK_URL are defined in secrets.h

// Rate limit (ms) for repeated notifications while sensor remains wet: default 5 minutes
const unsigned long RATE_LIMIT_MS = 5UL * 60UL * 1000UL; // 5 minutes


// Sensor pin (use a proper input pin for ESP32; GPIO22 is input-only)
const int SENSOR_PIN = 22;
// Onboard LED (GPIO2 on most ESP32 dev boards)
const int LED_PIN = 2;

// If your sensor outputs HIGH when water is present, set to HIGH; if it outputs LOW when water is present, set to LOW
const int SENSOR_ACTIVE_STATE = HIGH;

// Debounce settings
const unsigned long DEBOUNCE_MS = 50;

// Device name included in payload
const char* DEVICE_NAME = "ESP32-WaterSensor";

// ====== END USER CONFIG ======

// State tracking
int lastSensorReading = -1;
int stableSensorState = -1; // debounced state
unsigned long lastDebounceTime = 0;
unsigned long lastSendTime = 0;

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("ESP32 Water Sensor starting...");


  pinMode(SENSOR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // onboard LED off

  // Read initial state
  lastSensorReading = digitalRead(SENSOR_PIN);
  stableSensorState = lastSensorReading;

  connectWiFi();
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.printf("Connecting to WiFi '%s'...\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi connect failed (will retry in loop)");
  }
}

void loop() {
  // Ensure WiFi connected; try reconnecting in background
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  int reading = digitalRead(SENSOR_PIN);

  if (reading != lastSensorReading) {
    lastDebounceTime = millis();
  }

  // If the reading has been stable for DEBOUNCE_MS, take it as the actual state
  if ((millis() - lastDebounceTime) > DEBOUNCE_MS) {
    if (reading != stableSensorState) {
      stableSensorState = reading;
      // Log state change
      Serial.printf("Sensor stable state changed: %d\n", stableSensorState);

      // If it became active (water present) then conditionally send
      if (stableSensorState == SENSOR_ACTIVE_STATE) {
        unsigned long now = millis();
        if ((now - lastSendTime) >= RATE_LIMIT_MS || lastSendTime == 0) {
          // send notification
          bool ok = sendNotification();
          if (ok) {
            lastSendTime = now;
            // flash onboard LED briefly
            digitalWrite(LED_PIN, HIGH);
            delay(200);
            digitalWrite(LED_PIN, LOW);
          }
        } else {
          Serial.println("Water detected but rate-limited (skipping send)");
        }
      }
    }
  }

  lastSensorReading = reading;

  // small delay to avoid tight loop
  delay(10);
}


// Send alert to ntfy.innovorix.com/water_alert
bool sendNtfyAlert() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Not connected to WiFi; cannot send ntfy alert");
    return false;
  }

  HTTPClient http;
  http.begin(NTFY_URL);
  http.addHeader("Content-Type", "text/plain");

  String message = String(DEVICE_NAME) + " - WATER DETECTED at " + getISOTime();

  Serial.print("Sending ntfy alert to: "); Serial.println(NTFY_URL);
  Serial.print("Message: "); Serial.println(message);

  int httpCode = http.POST(message);
  if (httpCode > 0) {
    String resp = http.getString();
    Serial.printf("ntfy HTTP %d, response: %s\n", httpCode, resp.c_str());
    http.end();
    return (httpCode >= 200 && httpCode < 300);
  } else {
    Serial.printf("ntfy POST failed, error: %s\n", http.errorToString(httpCode).c_str());
    http.end();
    return false;
  }
}

// Main notification function (expandable)
bool sendNotification() {
  bool ok = true;
  // Send to ntfy
  ok &= sendNtfyAlert();
  // You can add more alert functions here in future
  return ok;
}

String getISOTime() {
  // Return ms-based approximate UTC timestamp string since we don't use NTP here.
  unsigned long now = millis();
  // convert to seconds since boot and append ms. Not a true calendar time but useful for debugging.
  unsigned long secs = now / 1000UL;
  unsigned long ms = now % 1000UL;
  char buf[64];
  snprintf(buf, sizeof(buf), "uptime_%lus_%03ums", secs, ms);
  return String(buf);
}
