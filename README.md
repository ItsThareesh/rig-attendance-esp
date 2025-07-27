# RIG Attendance System - WiFi and Time Synchronization

A comprehensive ESP32-based attendance system featuring dual WiFi operation, automatic time synchronization, and secure HMAC-based token generation for seamless attendance management.

## Overview

The system has been designed to operate in dual WiFi mode (AP+STA), eliminating the need for mode switching and ensuring continuous availability of both the captive portal and internet connectivity for time synchronization.

## Architecture
The system consists of several key components:
- Dual WiFi Mode (AP+STA): Simultaneous Access Point and Station operation
- Captive Portal: Web interface for user interaction
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

### 3. Intelligent Time Synchronization
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
│   ├── redirector.h
│   ├── time_sync.h
│   ├── wifi_config.h
│   └── wifi_softap.h
├── main
│   ├── CMakeLists.txt
│   ├── hmac_token_generator.cpp
│   ├── main.cpp
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
3. Begin continuous WiFi scanning for target network
4. Start time synchronization task (optimized for immediate sync)
5. Start web server for captive portal

### Runtime Operation
- **Access Point**: Always available for device configuration via captive portal
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
Generated tokens follow this format:
```
ts=1643723400&am=0&hmac=a1b2c3d4e5f6789..
```

## Benefits
1. **No Mode Switching**: Eliminates disruption to AP functionality during time sync
2. **Continuous Availability**: Captive portal always accessible
3. **Automatic Connection**: No manual intervention needed for WiFi connection
4. **Immediate Time Sync**: Time synchronizes as soon as internet connection is available
5. **Robust Reconnection**: Handles network outages and reconnections
6. **Efficient Scanning**: Smart scanning intervals based on connection status
7. **Asynchronous Operations**: Non-blocking time sync preserves system responsiveness
8. **Better Logging**: Comprehensive status reporting for debugging
9. **Smart Delay Management**: Avoids redundant sync operations
10. **Accurate Timestamps**: Ensures precise attendance logging
11. **Minimal Disruption**: SoftAP functionality remains uninterrupted
12. **Configurable Intervals**: Flexible time sync scheduling

## Setup Instructions

1. **Configure WiFi Credentials**: Edit `include/wifi_config.h` with your WiFi network details
2. **Change the following settings on ESP Menu-Config**
   - Enable **C++ Exceptions**
   - Change **Max HTTP Request Header Length** to `1024`
   - Change **Partition Table** to `Single factory app (large), no OTA`
3. **Build and Flash**: Compile and upload the firmware to your ESP32
4. **Automatic Operation**: The device will automatically:
   - Start the captive portal (ESP32-WIFI)
   - Scan for and connect to your configured WiFi network
   - Begin time synchronization once connected

## Monitoring and Debugging
Check the serial output for status messages:
- "Starting periodic time synchronization..."
- "Connected to WiFi for time sync"
- "Time synchronized successfully: [timestamp]"
- "Next time sync in X minutes"
- WiFi connection status updates
- Auto-reconnection attempts

## Usage Notes
- The ESP32 will appear as "ESP32-WIFI" access point immediately after startup
- When the configured WiFi network becomes available, the device will auto-connect
- Time sync will begin automatically once connected to the internet
- Captive portal functionality remains uninterrupted throughout operation (it's advised to sync with UNIX time to work with backend)
- Monitor serial logs to track WiFi connection and time sync status

## Troubleshooting

- If time sync fails, check WiFi credentials in `wifi_config.h`
- Verify that the target WiFi network is available and has internet access
- Check serial output for connection status and error messages
- Ensure NTP servers are accessible from your network