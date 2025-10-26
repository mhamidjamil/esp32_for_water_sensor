/*
  water_detect_deep_sleep.ino
  ESP32 simple water detection with deep sleep

  Approach:
  - Uses two ESP32 pins: one GPIO with internal pull-up, one GND
  - When water bridges the pins, GPIO goes LOW
  - Sends alert to ntfy and goes into deep sleep
  - Wakes up periodically to check again

  Hardware:
  - Connect one bare wire to GPIO pin (e.g., GPIO 25)
  - Connect another bare wire to GND pin
  - Dip both wires in water container
  - When water bridges the pins, alert is sent

  No external resistors needed - uses ESP32 internal pull-up resistor.
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include "secrets.h"

// ====== USER CONFIG ======
// Detection pin with internal pull-up (GPIO 25 recommended - stable input pin)
const int WATER_DETECT_PIN = 25;

// Deep sleep duration in microseconds (e.g., 10 seconds = 10 * 1000000)
// After sending alert, ESP32 sleeps for this duration before checking again
const uint64_t DEEP_SLEEP_DURATION_US = 10ULL * 1000000ULL; // 10 seconds

// Device name
const char* DEVICE_NAME = "ESP32-DirectWaterDetect";

// Onboard LED for visual feedback
const int LED_PIN = 2;

// ====== END USER CONFIG ======

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("=== ESP32 Water Detection (Direct + Deep Sleep) ===");

  // Setup LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // LED on during operation

  // Setup water detection pin with internal pull-up
  pinMode(WATER_DETECT_PIN, INPUT_PULLUP);

  // Small delay to stabilize reading
  delay(50);

  // Read pin state for immediate detection (covers cold boot or manual reset)
  // When dry: pull-up keeps pin HIGH
  // When wet (water bridges to GND): pin reads LOW
  int pinState = digitalRead(WATER_DETECT_PIN);

  Serial.printf("Water detect pin (GPIO %d) state: %s\n",
                WATER_DETECT_PIN,
                pinState == LOW ? "LOW (WATER DETECTED)" : "HIGH (DRY)");

  // Determine wake reason (if any) from deep sleep
  esp_sleep_wakeup_cause_t wakeReason = esp_sleep_get_wakeup_cause();
  switch (wakeReason) {
    case ESP_SLEEP_WAKEUP_EXT0:
      Serial.println("Woke from deep sleep by external (EXT0) wakeup - water pin triggered");
      break;
    case ESP_SLEEP_WAKEUP_TIMER:
      Serial.println("Woke from deep sleep by timer");
      break;
    case ESP_SLEEP_WAKEUP_UNDEFINED:
    default:
      Serial.println("Normal boot or unknown wakeup reason");
      break;
  }

  // Decide whether to send alert:
  // - If wake reason is EXT0 (pin triggered)
  // - Or if pin reads LOW right now (covers cold boot / reset while water present)
  bool shouldSendAlert = false;
  if (wakeReason == ESP_SLEEP_WAKEUP_EXT0) {
    shouldSendAlert = true;
  } else if (pinState == LOW) {
    // If pin is low at startup, treat as water detected and send alert
    shouldSendAlert = true;
  }

  if (shouldSendAlert) {
    Serial.println("Water detected! Connecting to WiFi and sending alert...");
    // Turn LED on while sending
    digitalWrite(LED_PIN, HIGH);

    if (connectWiFi()) {
      bool alertSent = sendWaterAlert();
      if (alertSent) {
        Serial.println("Alert sent successfully!");
        // Blink LED to indicate success
        for (int i = 0; i < 3; i++) {
          digitalWrite(LED_PIN, LOW);
          delay(200);
          digitalWrite(LED_PIN, HIGH);
          delay(200);
        }
      } else {
        Serial.println("Failed to send alert");
      }
    } else {
      Serial.println("WiFi connection failed - cannot send alert");
    }
  } else {
    Serial.println("No water detected. Going to deep sleep...");
  }

  // Turn off LED before sleep
  digitalWrite(LED_PIN, LOW);

  // Disconnect WiFi to save power
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  Serial.printf("Configuring deep sleep (pin + timer). Will sleep for %llu seconds unless pin triggers wake...\n", DEEP_SLEEP_DURATION_US / 1000000ULL);
  Serial.flush();

  // Configure EXT0 wake: wake when WATER_DETECT_PIN goes LOW (water bridges to GND)
  // Note: EXT0 requires the pin to be an RTC GPIO (GPIO25 is RTC-capable on most ESP32 boards)
  esp_sleep_enable_ext0_wakeup((gpio_num_t)WATER_DETECT_PIN, 0); // 0 = wake on LOW

  // Also set a timer wake as a fallback to periodically check
  esp_sleep_enable_timer_wakeup(DEEP_SLEEP_DURATION_US);

  // Enter deep sleep
  esp_deep_sleep_start();
}

void loop() {
  // Never reached - ESP32 restarts after deep sleep
}

bool connectWiFi() {
  Serial.printf("Connecting to WiFi '%s'...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
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
    return true;
  } else {
    Serial.println("WiFi connection failed");
    return false;
  }
}

bool sendWaterAlert() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Not connected to WiFi");
    return false;
  }

  HTTPClient http;
  http.begin(NTFY_URL);
  http.addHeader("Title", "Water Alert");
  http.addHeader("Priority", "high");
  http.addHeader("Tags", "droplet,warning");

  // Create message with boot count (shows how many times alert was sent)
  String message = String(DEVICE_NAME) + " - WATER DETECTED!";

  Serial.print("Sending alert to: ");
  Serial.println(NTFY_URL);
  Serial.print("Message: ");
  Serial.println(message);

  int httpCode = http.POST(message);

  if (httpCode > 0) {
    Serial.printf("HTTP Response: %d\n", httpCode);
    if (httpCode >= 200 && httpCode < 300) {
      Serial.println("Alert sent successfully");
      http.end();
      return true;
    }
  } else {
    Serial.printf("HTTP POST failed: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();
  return false;
}
