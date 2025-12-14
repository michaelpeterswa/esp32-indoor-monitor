# ESP32 Indoor Monitor

A WiFi-enabled indoor environmental monitoring system using ESP32-S3 and BME280 sensor. Provides temperature, humidity, and pressure data via a JSON API with automatic time synchronization.

## Features

- **BME280 Sensor Support** - Temperature, humidity, and atmospheric pressure monitoring
- **WiFi Configuration Portal** - No hardcoded credentials, configure via web interface
- **JSON REST API** - Access sensor data at `/api/v1/observation`
- **Web Dashboard** - Simple status page at root URL
- **NTP Time Sync** - Automatic time synchronization with timestamps in UTC and ISO8601
- **mDNS Support** - Access device via `esp32-monitor-{deviceid}.local`
- **CORS Enabled** - API accessible from browser-based applications
- **Unique Device IDs** - Each device gets a persistent 6-character identifier
- **Low Power Mode** - Optimized sensor sampling for weather monitoring

## Hardware Requirements

### Components
- **Waveshare ESP32-S3-Zero** (or compatible ESP32-S3 board)
- **BME280** sensor module (I2C)
- Connecting wires

### Wiring

| BME280 Pin | ESP32-S3 Pin | Description |
|------------|--------------|-------------|
| VCC        | 3V3          | Power       |
| GND        | GND          | Ground      |
| SCL        | GPIO 9       | I2C Clock   |
| SDA        | GPIO 8       | I2C Data    |

## Software Setup

### Prerequisites

- [PlatformIO](https://platformio.org/) (recommended) or Arduino IDE
- USB cable for initial programming

### Installation

1. Clone this repository:
```bash
git clone https://github.com/michaelpeterswa/esp32-indoor-monitor.git
cd esp32-indoor-monitor
```

2. Build and upload:
```bash
pio run -t upload
```

3. Monitor serial output (optional):
```bash
pio device monitor
```

## First-Time Setup

### WiFi Configuration

1. Power on the device
2. Look for WiFi network: `ESP32-Monitor-{deviceid}` (e.g., `ESP32-Monitor-nozlis`)
3. Connect to it (password: `password123`)
4. Captive portal should open automatically, or navigate to `http://192.168.4.1`
5. Select your WiFi network and enter the password
6. Device will save credentials and connect automatically on future boots

### Serial Output

On successful startup, you'll see:
```
Device: nozlis
IP: 192.168.1.100
Ready
```

### Resetting WiFi Settings

To reconfigure WiFi:
1. Uncomment line 184 in `src/main.cpp`: `wifiManager.resetSettings();`
2. Upload the code
3. Device will forget saved WiFi and start the configuration portal
4. Re-comment the line and upload again after configuration

## API Usage

### Endpoints

#### `GET /`
Simple web dashboard showing device information and API links.

**Response:** HTML page

---

#### `GET /api/v1/observation`
Returns current sensor readings with timestamps.

**Response Format:**
```json
{
  "device_id": "nozlis",
  "temperature_celsius": 23.5,
  "humidity_percent": 45.2,
  "pressure_hpa": 1013.25,
  "timestamp": 1702481445,
  "timestamp_iso": "2025-12-13T15:30:45Z",
  "last_read_ms": 12345
}
```

**Fields:**
- `device_id` - Unique 6-character device identifier
- `temperature_celsius` - Temperature in degrees Celsius
- `humidity_percent` - Relative humidity percentage (0-100)
- `pressure_hpa` - Atmospheric pressure in hectopascals
- `timestamp` - Unix timestamp (seconds since Jan 1, 1970 UTC)
- `timestamp_iso` - ISO8601 formatted timestamp (UTC)
- `last_read_ms` - Milliseconds since last sensor read

### Access Methods

1. **By IP Address:**
   ```bash
   curl http://192.168.1.100/api/v1/observation
   ```

2. **By mDNS hostname:**
   ```bash
   curl http://esp32-monitor-nozlis.local/api/v1/observation
   ```

3. **In Browser:**
   Navigate to `http://esp32-monitor-nozlis.local/` for the web dashboard

### Example Usage

**Python:**
```python
import requests

response = requests.get('http://esp32-monitor-nozlis.local/api/v1/observation')
data = response.json()

print(f"Temperature: {data['temperature_celsius']}°C")
print(f"Humidity: {data['humidity_percent']}%")
print(f"Pressure: {data['pressure_hpa']} hPa")
```

**JavaScript:**
```javascript
fetch('http://esp32-monitor-nozlis.local/api/v1/observation')
  .then(response => response.json())
  .then(data => {
    console.log(`Temperature: ${data.temperature_celsius}°C`);
    console.log(`Humidity: ${data.humidity_percent}%`);
    console.log(`Pressure: ${data.pressure_hpa} hPa`);
  });
```

**curl:**
```bash
curl -s http://esp32-monitor-nozlis.local/api/v1/observation | jq
```

## Configuration

### Timezone Settings

Edit `src/main.cpp` lines 18-19 to adjust timezone:

```cpp
const long gmtOffset_sec = 0;      // UTC offset in seconds
const int daylightOffset_sec = 0;  // Daylight saving offset
```

**Examples:**
- **EST (UTC-5):** `gmtOffset_sec = -5 * 3600;`
- **PST (UTC-8):** `gmtOffset_sec = -8 * 3600;`
- **CET (UTC+1):** `gmtOffset_sec = 1 * 3600;`

### Sensor Read Interval

Default: 5 seconds (line 26):
```cpp
const unsigned long delayTime = 5000;  // milliseconds
```

For lower power consumption, increase to 60000 (1 minute) as recommended for weather monitoring.

### Flash Size

Configured for 4MB flash in `platformio.ini`. If using different hardware, adjust:
```ini
board_upload.flash_size = 4MB
```

## Troubleshooting

### Sensor Not Found
```
ERROR: BME280 sensor not found!
```
**Solutions:**
- Verify I2C wiring (SDA → GPIO 8, SCL → GPIO 9)
- Check sensor power (3.3V)
- Ensure BME280 address is 0x76 or 0x77 (code tries both)
- Test I2C with `i2cdetect` if available

### WiFi Portal Not Appearing
- Ensure no saved WiFi credentials, or reset them (see "Resetting WiFi Settings")
- Check AP name: `ESP32-Monitor-{deviceid}`
- Connect within 3 minutes (portal timeout)

### mDNS Not Working
- Ensure device and client are on same network
- mDNS may not work across VLANs or with some routers
- Use IP address as fallback
- On Windows, install [Bonjour Print Services](https://support.apple.com/kb/DL999)

### Time Not Synchronizing
- Check internet connectivity
- NTP requires UDP port 123 outbound
- Time sync retries for 10 seconds, then continues with inaccurate time

## Technical Details

### Power Consumption
- **Active (WiFi on):** ~80-120mA
- **Sensor reading:** ~8ms per measurement
- **Deep sleep:** Not implemented (always-on design)

### Sensor Configuration
- **Mode:** Forced (sensor sleeps between readings)
- **Oversampling:** 1x for temperature, pressure, humidity
- **Filter:** Off
- **Accuracy:** ±1°C, ±3% RH, ±1 hPa

### Memory Usage
- **Flash:** ~1.2MB (30% of 4MB)
- **RAM:** ~40KB heap usage typical

## Over-The-Air (OTA) Updates

OTA update functionality is planned but not yet implemented. Current options:

1. **Arduino OTA** - Update via WiFi using PlatformIO
2. **Web OTA** - Upload firmware via web interface
3. **HTTP OTA** - Automatic updates from server

See future implementation in project roadmap.

## License

[Add your license here]

## Contributing

Contributions welcome! Please open an issue or submit a pull request.

## Acknowledgments

- [Adafruit BME280 Library](https://github.com/adafruit/Adafruit_BME280_Library)
- [WiFiManager](https://github.com/tzapu/WiFiManager)
- [ArduinoJson](https://arduinojson.org/)
- [PlatformIO](https://platformio.org/)
