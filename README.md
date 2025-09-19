# RIG Attendance System - WiFi, NFC, and Time Synchronization

A comprehensive ESP32-based attendance system featuring dual WiFi operation, NFC functionality with ST25DV sensor, automatic time synchronization, and secure HMAC-based token generation and updation utilizing interrupt signals for seamless attendance management.

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
  - SSID: `RIG-Attendance` (configurable in `wifi_ap_sta.h`)
  - Open authentication (no password)
  - IP: 192.168.4.1 (default)
  - Max connections: 4 (default)

- **Station (STA)**: Continuously scans for and connects to target WiFi network
  - Target network: Configured in `wifi_ap_sta.h` (WIFI_SSID_FOR_SYNC)
  - Password: Configured in `wifi_ap_sta.h` (WIFI_PASS_FOR_SYNC)
  - Auto-reconnection on disconnect

### 2. Automatic WiFi Network Discovery
- Scans every 60 seconds when connected to the target WiFi network
- If not connected, then retries every 15 seconds
- Automatically connects when the target network is found
- Maintains connection and handles disconnections gracefully

### 3. NFC Communication with ST25DV Sensor
- **Hardware**: ST25DV dynamic NFC/RFID tag with I2C interface
  - I2C Configuration: SDA on GPIO 21, SCL on GPIO 22 and GPO on GPIO 4 (Configurable in `nfc.h`)
  - Clock Speed: 1 MHz (as per ST25DV datasheet specification)
  - Dual interface: NFC wireless + I2C wired communication

- **NDEF Record Management**: 
  - Creates NDEF (NFC Data Exchange Format) records automatically
  - URI record: Attendance URL with embedded HMAC token
  - URL format: `webapp--rig-attendance-app.asia-east1.hosted.app/scan?[token]`

- **Token Integration**:
  - Generates fresh HMAC tokens every 5 seconds
  - Uses accessMethod = 1 for NFC-based tokens (vs 0 for web access)
  - Ensures unique token identification for different access methods

- **Interrupt-Driven Tap Detection**:
  - Monitors RF Field Activity using GPO pin (GPIO 4)
  - Writes NDEF records to sensor's EEPROM on tap detection

### 4. Intelligent Time Synchronization
- **Immediate Sync**: Automatically triggers time sync as soon as WiFi STA gets an IP address
- **Periodic Sync**: Continues to sync time every 10 minutes (configurable)
- **Smart Initial Delay**: Skips initial 30-second delay if immediate sync already occurred
- **Asynchronous Operation**: Immediate sync runs in background without blocking WiFi operations
- **Fallback Protection**: Only syncs when WiFi STA is connected to external network
- **Multiple NTP Servers**: Uses multiple NTP servers for redundancy:
  - pool.ntp.org
  - time.nist.gov  
  - time.google.com

## Project Structure
```
.
├── CMakeLists.txt
├── README.md
├── build/
├── components/
│   ├── dns_server
│   ├── hmac_token_generator
│   ├── nfc
│   ├── time_sync
│   └── wifi_connect
├── dependencies.lock
├── include/
├── main/
│   ├── CMakeLists.txt
│   ├── idf_component.yml
│   └── main.cpp
├── sdkconfig
└── sdkconfig.old
```

## Configuration

### WiFi Settings (`include/wifi_ap_sta.h`)
```c
// WiFi network credentials configuration
#define WIFI_SSID_FOR_SYNC "Your_WiFi_Network_Name"    // Target WiFi network
#define WIFI_PASS_FOR_SYNC "Your_WiFi_Password"        // WiFi password

// SoftAP configuration
#define WIFI_AP_SSID "RIG-Attendance"                  // AP SSID
#define WIFI_AP_CHANNEL 1                              // AP WiFi channel
#define WIFI_AP_MAX_CONNECTIONS 4                      // Max AP connections

// WiFi scan configuration
#define WIFI_SCAN_INTERVAL_MS 60000                    // Scan every 60 seconds
#define WIFI_CONNECT_RETRY_DELAY_MS 15000              // Retry connection every 15 seconds
```

### NFC Settings (`include/nfc.h`)
```c
// I2C Configuration for ST25DV
#define NFC_SDA_GPIO 21                    // I2C SDA pin
#define NFC_SCL_GPIO 22                    // I2C SCL pin
#define NFC_GPO_PIN 4                      // GPO pin for RF field activity detection

#define NFC_UPDATE_INTERVAL_MS 5000        // Token update every 5 seconds
```

### Time Sync Settings (`include/time_sync.h`)
```c
#define TIME_SYNC_INTERVAL_MINUTES 10     // Time sync interval
#define WIFI_CONNECT_TIMEOUT_SECONDS 30   // Connection timeout
#define TIME_SYNC_TIMEOUT_SECONDS 30      // SNTP sync timeout
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
  - Continuous NDEF record updates every 5 seconds with fresh tokens
  - Automatic I2C communication with ST25DV sensor on GPIO 21/22
  - Generates unique tokens with accessMethod=1 for NFC identification
- **Station Mode**: 
  - Continuously scans for target WiFi (15s intervals when disconnected)
  - Auto-connects when target network is found
  - Triggers immediate time sync when IP address is obtained
- **Time Sync**:
  - Immediate sync when WiFi STA connects and gets IP
  - Periodic sync every 10 minutes when STA connected
  - Skips sync when STA disconnected
  - Smart delay management to avoid redundant syncs
  - Logs connection status and sync results

### Status Monitoring Functions
- WiFi STA connection status: `is_wifi_sta_connected()`
- Current time validity: `is_time_valid()`
- Manual time sync (blocking): `trigger_manual_time_sync()`
- Asynchronous time sync (non-blocking): `trigger_async_time_sync()`

### Token Format
Generated tokens follow this format with access method differentiation:
```
ts=1643723400&am=0&hmac=a1b2c3d4e5f6789...    // Web access (Captive Portal)
ts=1643723400&am=1&hmac=a1b2c3d4e5f6789...    // NFC access (ST25DV sensor)
```

Where:
- `ts`: UNIX timestamp for token validation
- `am`: Access method (0 = Captive Portal, 1 = NFC)
- `hmac`: HMAC-SHA256 signature for security verification

## Benefits
1. **Dual Access Methods**: Both contactless NFC and web-based captive portal access
2. **Continuous Availability**: Captive portal and NFC always accessible
3. **Automatic Connection**: No manual intervention needed for WiFi connection
4. **Contactless Operation**: NFC provides touch-free attendance logging
5. **Immediate Time Sync**: Time synchronizes as soon as internet connection is available
6. **Robust Reconnection**: Handles network outages and reconnections
7. **Efficient Scanning**: Smart scanning intervals based on connection status
8. **Asynchronous Operations**: Non-blocking time sync preserves system responsiveness
9. **Better Logging**: Comprehensive status reporting for debugging
10. **Smart Delay Management**: Avoids redundant sync operations
11. **Accurate Timestamps**: Ensures precise attendance logging
12. **Minimal Disruption**: SoftAP functionality remains uninterrupted
13. **Configurable Intervals**: Flexible time sync scheduling
14. **Enhanced Security**: Access method differentiation in token generation
15. **Real-time Updates**: NFC tokens refresh every 5 seconds for enhanced security

## Setup Instructions

### Hardware Requirements
1. **ESP32 Development Board**
2. **ST25DV NFC/RFID Tag Module**
   - Connect SDA to GPIO 21
   - Connect SCL to GPIO 22
   - Connect GPO to GPIO 4
   - Connect VCC to 3.3V
   - Connect GND to Ground
   - Ensure proper I2C pull-up resistors (typically 4.7kΩ)

### Software Configuration
1. **Configure WiFi Credentials**: Edit `include/wifi_ap_sta.h` with your WiFi network details
2. **Change the following settings on ESP Menu-Config**
   - Change **Max HTTP Request Header Length** to `1024`
   - Change **Partition Table** to `Single factory app (large), no OTA`
   - Edit **configTIMER_TASK_STACK_DEPTH** to `4096`
3. **Install Dependencies**: The system automatically manages ESP component dependencies via `idf_component.yml`
4. **Build and Flash**: Compile and upload the firmware to your ESP32

### Automatic Operation
The device will automatically:
- Start the captive portal (RIG-Attendance)
- Initialize NFC communication with ST25DV sensor
- Scan for and connect to your configured WiFi network
- Begin time synchronization once connected
- Start NFC token updates and monitor tap events

## Monitoring and Debugging
Check the serial output for status messages:
- "Starting NFC task..."
- "I2C configured successfully for NFC"
- "NFC periodic update timer started (5s interval)"
- "Starting periodic time synchronization"
- "Time after sync: [timestamp]"
- "Next time sync in 10 minutes"
- WiFi connection status updates
- Auto-reconnection attempts

## Usage Notes
- The ESP32 will appear as "RIG-Attendance" access point immediately after startup
- NFC functionality is immediately available via ST25DV sensor once hardware is connected
- When the configured WiFi network becomes available, the device will auto-connect
- Time sync will begin automatically once connected to the internet
- Both captive portal and NFC functionality remain available throughout operation
- NFC tokens are automatically refreshed every 5 seconds for enhanced security
- Monitor serial logs to track WiFi connection, time sync, and NFC status
- It's advised to sync with UNIX time to work with backend

## Troubleshooting

### WiFi and Time Sync Issues
- If time sync fails, check WiFi credentials in `wifi_ap_sta.h`
- Verify that the target WiFi network is available and has internet access
- Check serial output for connection status and error messages
- Ensure NTP servers are accessible from your network

### NFC Related Issues
- If NFC initialization fails, verify ST25DV sensor connections (SDA/SCL/GPO on GPIO 21/22/4)
- Check I2C bus for proper pull-up resistors (4.7kΩ recommended)
- Ensure ST25DV sensor has adequate power supply (3.3V)
- Monitor serial output for "I2C configured successfully for NFC" message
- Check serial output for "GPO interrupt configured on GPIO 4" message
- Ensure that "Periodic Update Timer Started (5s Interval)" message appears in the logs