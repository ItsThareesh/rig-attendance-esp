#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Initializes the ESP32 in Wi-Fi AP+STA mode
void wifi_init_softap(void);

// Check if STA is connected to external WiFi network
bool is_wifi_sta_connected(void);

#ifdef __cplusplus
}
#endif