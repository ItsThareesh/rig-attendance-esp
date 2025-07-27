#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

// WiFi credentials for time synchronization
// Replace these with your actual WiFi network credentials
#define WIFI_SSID_FOR_SYNC "Thareesh’s iPhone 14 Pro"
#define WIFI_PASS_FOR_SYNC "getConnected"

// Time sync settings
#define TIME_SYNC_INTERVAL_MINUTES 10
#define WIFI_CONNECT_TIMEOUT_SECONDS 30
#define TIME_SYNC_TIMEOUT_SECONDS 30

// SNTP servers
#define SNTP_SERVER_1 "pool.ntp.org"
#define SNTP_SERVER_2 "time.nist.gov"
#define SNTP_SERVER_3 "time.google.com"

#endif // WIFI_CONFIG_H
