# RIG Attendance System - WiFi, NFC, and Time Synchronization

A comprehensive ESP32-based attendance system featuring dual WiFi operation, NFC functionality with ST25DV sensor, automatic time synchronization, and secure HMAC-based token generation for seamless attendance management.

## Overview

The system has been designed to operate in dual WiFi mode (AP+STA) with integrated NFC capabilities, eliminating the need for mode switching and ensuring continuous availability of both the captive portal and NFC-based attendance scanning. The system provides dual access methods for attendance logging while maintaining internet connectivity for time synchronization.

## Architecture
The system consists of several key components:
- Dual WiFi Mode (AP+STA): Simultaneous Access Point and Station operation
- Captive Portal: Web interface for user interaction
- NFC Communication: ST25DV sensor-based contactless attendance scanning
- HMAC Token Generator: Secure timestamp-based token creation using mbedTLS
- Time Synchronization: Automatic NTP-based time management
- DNS Server: Captive portal redirection handling

## Key Features

### 1. Dual Mode WiFi Operation (AP+STA)
- **Access Point (AP)**: Always active for captive portal functionality
  - SSID: `ESP32-WIFI` (configurable in `wifi_softap.cpp`)
  - Open authentication (no password)
  - IP: 192.168.4.1 (default)
  - Max connections: 4

- **Station (STA)**: Continuously scans for and connects to target WiFi network
  - Target network: Configured in `wifi_config.h` (WIFI_SSID_FOR_SYNC)
  - Password: Configured in `wifi_config.h` (WIFI_PASS_FOR_SYNC)
  - Auto-reconnection on disconnect

### 2. Automatic WiFi Network Discovery
- Scans for the target WiFi network every 30 seconds when not connected
- Automatically connects when the target network is found
- Retries connection every 5 seconds if connection fails
- Maintains connection and handles disconnections gracefully

### 3. NFC Communication with ST25DV Sensor
- **Hardware**: ST25DV dynamic NFC/RFID tag with I2C interface
  - I2C Configuration: SDA on GPIO 21, SCL on GPIO 22
  - Clock Speed: 1 MHz (as per ST25DV datasheet specification)
  - Dual interface: NFC wireless + I2C wired communication

- **NDEF Record Management**: 
  - Creates NDEF (NFC Data Exchange Format) records automatically
  - Text record: "RIG Attendance System"
  - URI record: Attendance URL with embedded HMAC token
  - URL format: `webapp--rig-attendance-app.asia-east1.hosted.app/scan?[token]`

- **Token Integration**:
  - Generates fresh HMAC tokens every 30 seconds
  - Uses accessMethod = 1 for NFC-based tokens (vs 0 for web access)
  - Ensures unique token identification for different access methods

- **Automatic Updates**:
  - Periodic NDEF record updates every 30 seconds
  - NFC tap detection monitoring every 2 seconds  
  - Continuous token refresh for enhanced security

### 4. Intelligent Time Synchronization
- **Immediate Sync**: Automatically triggers time sync as soon as WiFi STA gets an IP address
- **Periodic Sync**: Continues to sync time every 10 minutes (configurable)
- **Smart Initial Delay**: Skips initial 30-second delay if immediate sync already occurred
- **Asynchronous Operation**: Immediate sync runs in background without blocking WiFi operations
- **Manual Sync**: Manual sync capability through `trigger_manual_time_sync()`
- **Fallback Protection**: Only syncs when WiFi STA is connected to external network
- **Multiple NTP Servers**: Uses multiple NTP servers for redundancy:
  - pool.ntp.org
  - time.nist.gov  
  - time.google.com

## Configuration

<!-- ### NFC Settings (`main/nfc.cpp`)
```c
// I2C Configuration for ST25DV
#define NFC_SDA_GPIO 21                    // I2C SDA pin
#define NFC_SCL_GPIO 22                    // I2C SCL pin  
#define NFC_I2C_FREQ_HZ 1000000           // 1 MHz clock speed
#define NFC_UPDATE_INTERVAL_MS 30000      // Token update every 30 seconds
#define NFC_TAP_CHECK_INTERVAL_MS 2000    // Tap detection every 2 seconds
``` -->

### WiFi Settings (`include/wifi_config.h`)
```c
// Replace these with your actual WiFi network credentials
#define WIFI_SSID_FOR_SYNC "Your_WiFi_Network_Name"    // Target WiFi network
#define WIFI_PASS_FOR_SYNC "Your_WiFi_Password"        // WiFi password
#define TIME_SYNC_INTERVAL_MINUTES 10                  // Time sync interval
#define WIFI_CONNECT_TIMEOUT_SECONDS 30                // Connection timeout
#define TIME_SYNC_TIMEOUT_SECONDS 30                   // SNTP sync timeout
```

### Access Point Settings (`main/wifi_softap.cpp`)
```c
#define ESP_WIFI_SSID "ESP32-WIFI"              // AP SSID
#define ESP_WIFI_CHANNEL 1                      // AP WiFi channel
#define MAX_STA_CONN 4                          // Max AP connections
```

### Time Sync Configuration Options
- `TIME_SYNC_INTERVAL_MINUTES`: How often to sync time (default: 10 minutes)
- `WIFI_CONNECT_TIMEOUT_SECONDS`: WiFi connection timeout (default: 30 seconds)
- `TIME_SYNC_TIMEOUT_SECONDS`: SNTP sync timeout (default: 30 seconds)

## Project Structure

```
.
├── CMakeLists.txt
├── README.md
├── components
│   └── dns_server
├── include
│   ├── hmac_token_generator.h
│   ├── nfc.h
│   ├── redirector.h
│   ├── time_sync.h
│   ├── wifi_config.h
│   └── wifi_softap.h
├── main
│   ├── CMakeLists.txt
│   ├── hmac_token_generator.cpp
│   ├── idf_component.yml
│   ├── main.cpp
│   ├── nfc.cpp
│   ├── redirector.cpp
│   ├── root.html
│   ├── time_sync.cpp
│   ├── token-generator.cpp
│   └── wifi_softap.cpp
└── sdkconfig
```


## How It Works

### Startup Sequence
1. Initialize WiFi in AP+STA mode
2. Start Access Point for captive portal
3. Initialize NFC communication with ST25DV sensor
4. Begin continuous WiFi scanning for target network
5. Start time synchronization task (optimized for immediate sync)
6. Start web server for captive portal
7. Start NFC periodic update and tap detection timers

### Runtime Operation
- **Access Point**: Always available for device configuration via captive portal
- **NFC Communication**:
  - Continuous NDEF record updates every 30 seconds with fresh tokens
  - NFC tap detection monitoring every 2 seconds
  - Automatic I2C communication with ST25DV sensor on GPIO 21/22
  - Generates unique tokens with accessMethod=1 for NFC identification
- **Station Mode**: 
  - Continuously scans for target WiFi (30s intervals when disconnected)
  - Auto-connects when target network is found
  - Retries connection on failures (5s intervals)
  - **Triggers immediate time sync when IP address is obtained**
- **Time Sync**: 
  - **Immediate sync** when WiFi STA connects and gets IP
  - Periodic sync every 10 minutes when STA connected
  - Skips sync when STA disconnected
  - Smart delay management to avoid redundant syncs
  - Logs connection status and sync results

### Status Monitoring Functions
- WiFi STA connection status: `is_wifi_sta_connected()`
- Time synchronization status: `is_time_synchronized()`
- Current time validity: `is_time_valid()`
- Manual time sync (blocking): `trigger_manual_time_sync()`
- Asynchronous time sync (non-blocking): `trigger_async_time_sync()`

### Token Format
Generated tokens follow this format with access method differentiation:
```
ts=1643723400&am=0&hmac=a1b2c3d4e5f6789..    // Web access (captive portal)
ts=1643723400&am=1&hmac=a1b2c3d4e5f6789..    // NFC access (ST25DV sensor)
```

Where:
- `ts`: Unix timestamp for token validation
- `am`: Access method (0 = Web/Captive Portal, 1 = NFC)
- `hmac`: HMAC-SHA256 signature for security verification

## Benefits
1. **No Mode Switching**: Eliminates disruption to AP functionality during time sync
2. **Dual Access Methods**: Both contactless NFC and web-based captive portal access
3. **Continuous Availability**: Captive portal and NFC always accessible
4. **Automatic Connection**: No manual intervention needed for WiFi connection
5. **Contactless Operation**: NFC provides touch-free attendance logging
6. **Immediate Time Sync**: Time synchronizes as soon as internet connection is available
7. **Robust Reconnection**: Handles network outages and reconnections
8. **Efficient Scanning**: Smart scanning intervals based on connection status
9. **Asynchronous Operations**: Non-blocking time sync preserves system responsiveness
10. **Better Logging**: Comprehensive status reporting for debugging
11. **Smart Delay Management**: Avoids redundant sync operations
12. **Accurate Timestamps**: Ensures precise attendance logging
13. **Minimal Disruption**: SoftAP functionality remains uninterrupted
14. **Configurable Intervals**: Flexible time sync scheduling
15. **Enhanced Security**: Access method differentiation in token generation
16. **Real-time Updates**: NFC tokens refresh every 30 seconds for enhanced security

## Setup Instructions

### Hardware Requirements
1. **ESP32 Development Board**
2. **ST25DV NFC/RFID Tag Module**
   - Connect SDA to GPIO 21
   - Connect SCL to GPIO 22
   - Connect VCC to 3.3V
   - Connect GND to Ground
   - Ensure proper I2C pull-up resistors (typically 4.7kΩ)

### Software Configuration
1. **Configure WiFi Credentials**: Edit `include/wifi_config.h` with your WiFi network details
2. **Change the following settings on ESP Menu-Config**
   - Enable **C++ Exceptions**
   - Change **Max HTTP Request Header Length** to `1024`
   - Change **Partition Table** to `Single factory app (large), no OTA`
3. **Install Dependencies**: The system automatically manages ESP component dependencies via `idf_component.yml`
4. **Build and Flash**: Compile and upload the firmware to your ESP32

### Automatic Operation
The device will automatically:
- Start the captive portal (ESP32-WIFI)
- Initialize NFC communication with ST25DV sensor
- Scan for and connect to your configured WiFi network
- Begin time synchronization once connected
- Start NFC token updates and tap detection

## Monitoring and Debugging
Check the serial output for status messages:
- "Starting NFC task..."
- "I2C configured successfully for NFC"
- "NFC periodic update timer started (30s interval)"
- "NDEF records written successfully with token"
- "Starting periodic time synchronization..."
- "Connected to WiFi for time sync"
- "Time synchronized successfully: [timestamp]"
- "Next time sync in X minutes"
- WiFi connection status updates
- Auto-reconnection attempts

## Usage Notes
- The ESP32 will appear as "ESP32-WIFI" access point immediately after startup
- NFC functionality is immediately available via ST25DV sensor once hardware is connected
- When the configured WiFi network becomes available, the device will auto-connect
- Time sync will begin automatically once connected to the internet
- Both captive portal and NFC functionality remain available throughout operation
- NFC tokens are automatically refreshed every 30 seconds for enhanced security
- Monitor serial logs to track WiFi connection, time sync, and NFC status
- It's advised to sync with UNIX time to work with backend

## Troubleshooting

### WiFi and Time Sync Issues
- If time sync fails, check WiFi credentials in `wifi_config.h`
- Verify that the target WiFi network is available and has internet access
- Check serial output for connection status and error messages
- Ensure NTP servers are accessible from your network

### NFC Related Issues
- If NFC initialization fails, verify ST25DV sensor connections (SDA/SCL on GPIO 21/22)
- Check I2C bus for proper pull-up resistors (4.7kΩ recommended)
- Ensure ST25DV sensor has adequate power supply (3.3V)
- Monitor serial output for "I2C configured successfully for NFC" message
- If NDEF write fails, check sensor compatibility and I2C communication
- Verify that NFC tokens are being updated every 30 seconds in serial logs