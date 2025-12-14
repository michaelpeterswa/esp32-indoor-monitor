#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <ArduinoJson.h>
#include <time.h>
#include <Preferences.h>

// I2C pins for ESP32-S3-Zero
#define I2C_SDA 8
#define I2C_SCL 9

// NTP settings
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 0;

Adafruit_BME280 bme;
WebServer server(80);
Preferences preferences;

String deviceId;
const unsigned long delayTime = 5000;
unsigned long lastSensorRead = 0;

// Cached sensor values
float temperature = 0.0;
float humidity = 0.0;
float pressure = 0.0;

void handleObservation()
{
    // Add CORS headers for browser-based apps
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");

    JsonDocument doc;

    doc["device_id"] = deviceId;
    doc["temperature_celsius"] = temperature;
    doc["humidity_percent"] = humidity;
    doc["pressure_hpa"] = pressure;

    // Get current time as Unix timestamp
    time_t now;
    time(&now);
    doc["timestamp"] = now;

    // Also add human-readable ISO8601 format
    struct tm timeinfo;
    if (getLocalTime(&timeinfo))
    {
        char timeString[30];
        strftime(timeString, sizeof(timeString), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
        doc["timestamp_iso"] = timeString;
    }

    // Add last sensor read timestamp
    doc["last_read_ms"] = lastSensorRead;

    String response;
    serializeJson(doc, response);

    server.send(200, "application/json", response);
}

void handleRoot()
{
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>body{font-family:Arial,sans-serif;margin:40px;background:#f0f0f0;}";
    html += ".container{background:white;padding:20px;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1);}";
    html += "h1{color:#333;}a{color:#0066cc;text-decoration:none;}";
    html += ".info{margin:10px 0;padding:10px;background:#f9f9f9;border-left:3px solid #0066cc;}</style>";
    html += "</head><body><div class='container'>";
    html += "<h1>ESP32 Indoor Monitor</h1>";
    html += "<div class='info'><strong>Device ID:</strong> " + deviceId + "</div>";
    html += "<div class='info'><strong>Hostname:</strong> esp32-monitor-" + deviceId + ".local</div>";
    html += "<div class='info'><strong>IP Address:</strong> " + WiFi.localIP().toString() + "</div>";
    html += "<h2>API Endpoints</h2>";
    html += "<div class='info'><a href='/api/v1/observation'>/api/v1/observation</a> - Get sensor data (JSON)</div>";
    html += "</div></body></html>";

    server.send(200, "text/html", html);
}

void handleNotFound()
{
    server.send(404, "text/plain", "Not Found");
}

void readSensorValues()
{
    temperature = bme.readTemperature();
    humidity = bme.readHumidity();
    pressure = bme.readPressure() / 100.0F;
}

String generateDeviceId()
{
    const char charset[] = "abcdefghijklmnopqrstuvwxyz";
    String id = "";

    // Use ESP32's unique chip ID as seed
    uint64_t chipid = ESP.getEfuseMac();
    randomSeed(chipid);

    for (int i = 0; i < 6; i++)
    {
        id += charset[random(0, 26)];
    }

    return id;
}

String getOrCreateDeviceId()
{
    preferences.begin("device", false);

    String storedId = "";

    // Check if key exists before reading to avoid NVS warning
    if (preferences.isKey("id"))
    {
        storedId = preferences.getString("id", "");
    }

    if (storedId.length() == 0)
    {
        // Generate new ID
        storedId = generateDeviceId();
        preferences.putString("id", storedId);
    }

    preferences.end();
    return storedId;
}

void setup()
{
    Serial.begin(115200);
    delay(100);

    // Get or create unique device ID
    deviceId = getOrCreateDeviceId();

    // Initialize I2C with custom pins
    Wire.begin(I2C_SDA, I2C_SCL);

    // Try both common BME280 addresses
    if (!bme.begin(0x76, &Wire))
    {
        if (!bme.begin(0x77, &Wire))
        {
            Serial.println("ERROR: BME280 sensor not found!");
            while (1)
                delay(1000);
        }
    }

    // Configure for weather/indoor monitoring - lower power, adequate accuracy
    bme.setSampling(Adafruit_BME280::MODE_FORCED,
                    Adafruit_BME280::SAMPLING_X1,  // temperature
                    Adafruit_BME280::SAMPLING_X1,  // pressure
                    Adafruit_BME280::SAMPLING_X1,  // humidity
                    Adafruit_BME280::FILTER_OFF);

    // Create unique AP name and hostname with device ID
    String apName = "ESP32-Monitor-" + deviceId;
    String hostname = "esp32-monitor-" + deviceId;

    // Set WiFi hostname BEFORE connecting
    WiFi.setHostname(hostname.c_str());

    // Initialize WiFiManager
    WiFiManager wifiManager;

    // Configure WiFiManager for faster response
    wifiManager.setConfigPortalTimeout(180); // 3 minute timeout
    wifiManager.setConnectTimeout(20);       // 20 second connection timeout
    wifiManager.setAPStaticIPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));

    // Uncomment to reset saved WiFi settings (useful for testing)
    // wifiManager.resetSettings();

    // Auto-connect to saved WiFi or start config portal
    if (!wifiManager.autoConnect(apName.c_str(), "password123"))
    {
        delay(3000);
        ESP.restart();
    }

    Serial.print("Device: ");
    Serial.println(deviceId);
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    // Configure NTP time
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    // Wait for time to be set
    struct tm timeinfo;
    int retries = 0;
    while (!getLocalTime(&timeinfo) && retries < 10)
    {
        delay(1000);
        retries++;
    }

    // Setup mDNS for easy access
    if (MDNS.begin(hostname.c_str()))
    {
        MDNS.addService("http", "tcp", 80);
    }

    // Setup web server routes
    server.on("/", HTTP_GET, handleRoot);
    server.on("/api/v1/observation", HTTP_GET, handleObservation);
    server.onNotFound(handleNotFound);

    server.begin();
    Serial.println("Ready");
}

void loop()
{
    server.handleClient();

    unsigned long currentMillis = millis();
    if (currentMillis - lastSensorRead >= delayTime)
    {
        lastSensorRead = currentMillis;
        bme.takeForcedMeasurement();
        readSensorValues();
    }
}