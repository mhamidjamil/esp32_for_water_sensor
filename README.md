# ESP32 Water Sensor

## What this is
- An Arduino sketch for ESP32 that monitors a digital water-touch sensor and sends a webhook HTTP POST when water is detected.
- It debounces the sensor and rate-limits notifications to once every 5 minutes while the sensor stays wet.

## Files
- `v1/water_sensor.ino` — the sketch to upload to your ESP32
- `secrets.example.h` — template for secrets (Wi-Fi, webhook); copy to `secrets.h` and fill in your credentials

## Quick setup
1. Copy `secrets.example.h` (in this directory) to `secrets.h` and fill in your Wi-Fi and webhook credentials.
   - Example:
     ```c
     #define WIFI_SSID "your_wifi"
     #define WIFI_PASS "your_password"
     #define WEBHOOK_URL "https://your.webhook.url"
     ```
   - Do NOT commit `secrets.h` to source control (add it to `.gitignore`).
2. Edit `v1/water_sensor.ino` if needed:
   - `SENSOR_PIN` if you use a different GPIO
   - `SENSOR_ACTIVE_STATE` to match your sensor (set to `HIGH` or `LOW` depending on whether the sensor outputs HIGH when water is present)
   - Optionally change `RATE_LIMIT_MS` (default 5 minutes)
3. Install Arduino ESP32 support in Arduino IDE (or use PlatformIO):
   - In Arduino IDE: File > Preferences > Additional Boards Manager URLs add: https://dl.espressif.com/dl/package_esp32_index.json
   - Tools > Board > Boards Manager > search "esp32" and install "esp32 by Espressif Systems"
4. Select the correct board (e.g., "ESP32 Dev Module"), select correct COM port, compile and upload `v1/water_sensor.ino`.

## Webhook / mobile notification options
- **IFTTT Webhooks:** Easy to configure. Create an applet with "Receive a web request" trigger and a notification action (or call other services). Use the Maker Webhooks URL as `WEBHOOK_URL`.
- **Pushover:** Paid small fee once; has a simple API for push notifications. You'd post to Pushover API with user/token in the payload.
- **Telegram bot:** Have your ESP32 call your server or a small cloud function which uses a bot token to send messages to your chat. Telegram doesn't accept plain webhooks with an open URL unless you host a relay.
- **Firebase Cloud Messaging (FCM):** More complex (requires server or cloud function), but reliable.

### Recommendation
- For easiest mobile notifications with zero server code: use IFTTT Webhooks + IFTTT Notifications (or IFTTT -> SMS/Email) or Pushover.
  - IFTTT: Free and simple to set up. Low rate of messages is fine.
  - Pushover: More direct, immediate push notification to phone.

## Testing
- Open Serial Monitor at 115200 baud to see Wi‑Fi and sensor logs.
- Simulate touching the sensor and watch for a POST being sent; you should see HTTP status logged.

## Notes & troubleshooting
- Some water sensors are "active LOW". If the sketch never sends when you touch the sensor, try changing `SENSOR_ACTIVE_STATE` to `LOW`.
- Use a stable VCC/GND reference; floating inputs may cause false triggers — consider adding a pull-down/up resistor if needed.
- The sketch uses a simple uptime timestamp instead of real-time. For accurate timestamps you can add NTP time retrieval.

## Secrets management
- Your Wi-Fi and webhook credentials are now stored in `secrets.h` (in this directory).
- Copy `secrets.example.h` to `secrets.h` and fill in your real credentials.
- Do NOT commit `secrets.h` to source control (add it to `.gitignore`).
- The sketch includes `../secrets.h` for credentials.

## License
- Provided as-is. You can modify it for your project.
