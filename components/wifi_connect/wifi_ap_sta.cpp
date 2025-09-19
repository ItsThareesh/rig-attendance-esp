#include <string.h>

#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "lwip/inet.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "wifi_ap_sta.h"
#include "time_sync.h"

static const char *TAG = "WIFI_AP";
static const char *TAG2 = "WIFI_STA";

static EventGroupHandle_t wifi_event_group;
static TaskHandle_t wifi_scan_task_handle = NULL;
static bool sta_connected = false;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    // Handle AP events
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "Station " MACSTR " Connected", MAC2STR(event->mac));
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "Station " MACSTR " Disconnected", MAC2STR(event->mac));
    }
    // Handle STA events
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG2, "WiFi started, beginning scan for target network");
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
    {
        wifi_event_sta_connected_t *event = (wifi_event_sta_connected_t *)event_data;
        ESP_LOGI(TAG2, "Connected to WiFi network: %s", event->ssid);
        sta_connected = true;
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGI(TAG2, "Disconnected from WiFi network (reason: %d)", event->reason);
        sta_connected = false;
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG2, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));

        // Trigger immediate manual time synchronization when connected
        ESP_LOGI(TAG2, "Triggering immediate time synchronization");

        if (trigger_async_time_sync() != ESP_OK)
            ESP_LOGW(TAG2, "Failed immediate time synchronization");
    }
}

static void wifi_scan_and_connect_task(void *pvParameters)
{
    ESP_LOGI(TAG2, "WiFi scan task started");

    while (1)
    {
        if (sta_connected)
        {
            ESP_LOGI(TAG2, "STA already connected, next scan in %d seconds", WIFI_SCAN_INTERVAL_MS / 1000);
            vTaskDelay(pdMS_TO_TICKS(WIFI_SCAN_INTERVAL_MS));
        }
        else
        {
            // This takes considerably long time, so measure duration for accurate delay
            uint32_t start_time = esp_log_timestamp();

            ESP_LOGI(TAG2, "Scanning for WiFi network: %s", WIFI_SSID_FOR_SYNC);

            // Start WiFi scan
            wifi_scan_config_t scan_config = {
                .ssid = (uint8_t *)WIFI_SSID_FOR_SYNC,
                .bssid = NULL,
                .channel = 0,
                .show_hidden = false,
                .scan_type = WIFI_SCAN_TYPE_ACTIVE,
                .scan_time = {
                    .active = {
                        .min = 100,
                        .max = 300,
                    },
                }};

            esp_err_t scan_result = esp_wifi_scan_start(&scan_config, true);
            if (scan_result == ESP_OK)
            {
                uint16_t ap_count = 0;
                esp_wifi_scan_get_ap_num(&ap_count);

                if (ap_count > 0)
                {
                    ESP_LOGI(TAG2, "Target WiFi network found! Attempting to connect...");

                    // Configure WiFi STA settings
                    wifi_config_t wifi_config = {
                        .sta = {
                            .ssid = WIFI_SSID_FOR_SYNC,
                            .password = WIFI_PASS_FOR_SYNC,
                            .threshold = {
                                .authmode = WIFI_AUTH_WPA2_PSK,
                            },
                            .pmf_cfg = {
                                .capable = true,
                                .required = false,
                            }}};

                    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);

                    // Try to connect
                    esp_err_t connect_result = esp_wifi_connect();
                    if (connect_result == ESP_OK)
                        ESP_LOGI(TAG2, "WiFi connection initiated");
                    else
                        ESP_LOGW(TAG2, "Failed to initiate WiFi connection: %s", esp_err_to_name(connect_result));
                }
                else
                    ESP_LOGD(TAG2, "Target WiFi network not found in scan results");
            }
            else
                ESP_LOGW(TAG2, "WiFi scan failed: %s", esp_err_to_name(scan_result));

            uint32_t scan_duration = esp_log_timestamp() - start_time;

            // If scan took less than retry delay, wait the remaining time, otherwise retry immediately
            uint32_t delay_time = (scan_duration < WIFI_CONNECT_RETRY_DELAY_MS) ? (WIFI_CONNECT_RETRY_DELAY_MS - scan_duration) : 0;

            vTaskDelay(pdMS_TO_TICKS(delay_time));
        }
    }
}

bool is_sta_connected(void) { return sta_connected; }

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
            .ssid = WIFI_AP_SSID,
            .password = "",
            .ssid_len = strlen(WIFI_AP_SSID),
            .channel = WIFI_AP_CHANNEL,
            .authmode = WIFI_AUTH_OPEN,
            .max_connection = WIFI_AP_MAX_CONNECTIONS,
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
    ESP_LOGI(TAG, "WiFi initialized in AP+STA mode. AP SSID: '%s'", WIFI_AP_SSID);
    ESP_LOGI(TAG2, "Will scan for STA network: '%s'", WIFI_SSID_FOR_SYNC);

    // Start the WiFi scan task (will be suspended when connected)
    if (wifi_scan_task_handle == NULL)
    {
        xTaskCreate(wifi_scan_and_connect_task, "wifi_scan_task", 4096, NULL, 5, &wifi_scan_task_handle);
    }
}
