#pragma once

// WiFi credentials for time synchronization
#define WIFI_SSID_FOR_SYNC "Thareesh’s iPhone 14 Pro"
#define WIFI_PASS_FOR_SYNC "getConnected"

// SoftAP configuration
#define WIFI_AP_SSID "RIG-Attendance"
#define WIFI_AP_PASS ""
#define WIFI_AP_CHANNEL 1
#define WIFI_AP_MAX_CONNECTIONS 4

// WiFi scan configuration
#define WIFI_SCAN_INTERVAL_MS 60000       // Scan every 60 seconds
#define WIFI_CONNECT_RETRY_DELAY_MS 15000 // Retry connection every 15 seconds

#ifdef __cplusplus
extern "C"
{
#endif

    // Function declarations
    // Initializes the ESP32 in Wi-Fi AP+STA mode
    void wifi_init_softap(void);

    // Check if STA is connected to external WiFi network
    bool is_sta_connected(void);

#ifdef __cplusplus
}
#endif
