#include <string.h>
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "lwip/inet.h"
#include "esp_log.h"
#include "wifi_softap.h"
#include "wifi_config.h"
#include "time_sync.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#define ESP_WIFI_SSID "ESP32-WIFI"
#define ESP_WIFI_CHANNEL 1
#define MAX_STA_CONN 4

// WiFi scan and connection settings
#define WIFI_SCAN_INTERVAL_MS 30000  // Scan every 30 seconds
#define WIFI_CONNECT_RETRY_DELAY_MS 5000  // Retry connection every 5 seconds

// Event group bits
#define WIFI_STA_CONNECTED_BIT BIT0
#define WIFI_STA_DISCONNECTED_BIT BIT1

static const char *TAG = "wifi_ap_sta";
static EventGroupHandle_t wifi_event_group;
static TaskHandle_t wifi_scan_task_handle = NULL;
static bool sta_connected = false;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    // Handle AP events
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "AP: Station " MACSTR " Connected", MAC2STR(event->mac));
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "AP: Station " MACSTR " Disconnected", MAC2STR(event->mac));
    }
    // Handle STA events
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG, "STA: WiFi started, beginning scan for target network");
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
    {
        wifi_event_sta_connected_t *event = (wifi_event_sta_connected_t *)event_data;
        ESP_LOGI(TAG, "STA: Connected to WiFi network: %s", event->ssid);
        sta_connected = true;
        xEventGroupSetBits(wifi_event_group, WIFI_STA_CONNECTED_BIT);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGI(TAG, "STA: Disconnected from WiFi network (reason: %d)", event->reason);
        sta_connected = false;
        xEventGroupSetBits(wifi_event_group, WIFI_STA_DISCONNECTED_BIT);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "STA: Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        
        // Trigger immediate asynchronous time synchronization when connected
        ESP_LOGI(TAG, "STA: Triggering immediate time synchronization...");
        esp_err_t sync_result = trigger_async_time_sync();
        if (sync_result == ESP_OK)
        {
            ESP_LOGI(TAG, "STA: Immediate time sync task created successfully");
        }
        else
        {
            ESP_LOGW(TAG, "STA: Failed to create immediate time sync task");
        }
    }
}

// Task to continuously scan for and connect to target WiFi network
static void wifi_scan_and_connect_task(void *pvParameters)
{
    ESP_LOGI(TAG, "WiFi scan and connect task started");
    
    while (1)
    {
        if (!sta_connected)
        {
            ESP_LOGI(TAG, "Scanning for WiFi network: %s", WIFI_SSID_FOR_SYNC);
            
            // Start WiFi scan
            wifi_scan_config_t scan_config = {
                .ssid = (uint8_t*)WIFI_SSID_FOR_SYNC,
                .bssid = NULL,
                .channel = 0,
                .show_hidden = false,
                .scan_type = WIFI_SCAN_TYPE_ACTIVE,
                .scan_time = {
                    .active = {
                        .min = 100,
                        .max = 300
                    }
                }
            };
            
            esp_err_t scan_result = esp_wifi_scan_start(&scan_config, true);
            if (scan_result == ESP_OK)
            {
                uint16_t ap_count = 0;
                esp_wifi_scan_get_ap_num(&ap_count);
                
                if (ap_count > 0)
                {
                    ESP_LOGI(TAG, "Target WiFi network found! Attempting to connect...");
                    
                    // Configure WiFi STA settings
                    wifi_config_t wifi_config = {};
                    strcpy((char *)wifi_config.sta.ssid, WIFI_SSID_FOR_SYNC);
                    strcpy((char *)wifi_config.sta.password, WIFI_PASS_FOR_SYNC);
                    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
                    wifi_config.sta.pmf_cfg.capable = true;
                    wifi_config.sta.pmf_cfg.required = false;
                    
                    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
                    
                    // Try to connect
                    esp_err_t connect_result = esp_wifi_connect();
                    if (connect_result == ESP_OK)
                    {
                        ESP_LOGI(TAG, "WiFi connection initiated");
                    }
                    else
                    {
                        ESP_LOGW(TAG, "Failed to initiate WiFi connection: %s", esp_err_to_name(connect_result));
                    }
                }
                else
                {
                    ESP_LOGD(TAG, "Target WiFi network not found in scan results");
                }
            }
            else
            {
                ESP_LOGW(TAG, "WiFi scan failed: %s", esp_err_to_name(scan_result));
            }
        }
        else
        {
            ESP_LOGD(TAG, "Already connected to WiFi, skipping scan");
        }
        
        // Wait before next scan attempt
        vTaskDelay(pdMS_TO_TICKS(sta_connected ? WIFI_SCAN_INTERVAL_MS : WIFI_CONNECT_RETRY_DELAY_MS));
    }
}

bool is_wifi_sta_connected(void)
{
    return sta_connected;
}

void wifi_init_softap(void)
{
    // Create event group
    wifi_event_group = xEventGroupCreate();
    
    // Initialize network interface and event loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Create default WiFi AP and STA interfaces
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();
    
    // Initialize WiFi with default configuration
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers for both AP and STA events
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    // Configure WiFi AP settings
    wifi_config_t ap_config = {
        .ap = {
            .ssid = ESP_WIFI_SSID,
            .password = "",
            .ssid_len = strlen(ESP_WIFI_SSID),
            .channel = ESP_WIFI_CHANNEL,
            .authmode = WIFI_AUTH_OPEN,
            .max_connection = MAX_STA_CONN,
        },
    };

    // Set WiFi mode to AP+STA
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Get and display AP IP address
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info);

    char ip_addr[16];
    inet_ntoa_r(ip_info.ip.addr, ip_addr, 16);
    ESP_LOGI(TAG, "Set up softAP with IP: %s", ip_addr);
    ESP_LOGI(TAG, "WiFi initialized in AP+STA mode. AP SSID: '%s'", ESP_WIFI_SSID);
    ESP_LOGI(TAG, "Will scan for STA network: '%s'", WIFI_SSID_FOR_SYNC);
    
    // Start the WiFi scan and connect task
    if (wifi_scan_task_handle == NULL)
    {
        xTaskCreate(wifi_scan_and_connect_task, "wifi_scan_task", 4096, NULL, 5, &wifi_scan_task_handle);
        ESP_LOGI(TAG, "WiFi scan and connect task started");
    }
}
