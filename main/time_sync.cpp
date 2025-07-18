#include "time_sync.h"
#include "wifi_config.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <time.h>
#include <sys/time.h>
#include <string.h>

static const char *TAG = "time_sync";

// Time sync interval (configurable)
#define TIME_SYNC_INTERVAL_MS (TIME_SYNC_INTERVAL_MINUTES * 60 * 1000)

// WiFi connection timeout (configurable)
#define WIFI_CONNECT_TIMEOUT_MS (WIFI_CONNECT_TIMEOUT_SECONDS * 1000)

// Event group bits
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define TIME_SYNC_DONE_BIT BIT2

static EventGroupHandle_t wifi_event_group;
static TaskHandle_t time_sync_task_handle = NULL;
static bool time_synchronized = false;

static void wifi_sta_event_handler(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGI(TAG, "WiFi STA disconnected");
        xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Time synchronized via SNTP");
    time_synchronized = true;
    xEventGroupSetBits(wifi_event_group, TIME_SYNC_DONE_BIT);
}

static esp_err_t connect_to_wifi_for_sync(void)
{
    ESP_LOGI(TAG, "Connecting to WiFi for time sync...");

    // Clear event bits
    xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT | TIME_SYNC_DONE_BIT);

    // Set WiFi mode to STA+AP
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    // Configure WiFi for STA
    wifi_config_t wifi_config = {};
    strcpy((char *)wifi_config.sta.ssid, WIFI_SSID_FOR_SYNC);
    strcpy((char *)wifi_config.sta.password, WIFI_PASS_FOR_SYNC);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Wait for connection or timeout
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));

    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "Connected to WiFi for time sync");
        return ESP_OK;
    }
    else
    {
        ESP_LOGW(TAG, "Failed to connect to WiFi for time sync");
        return ESP_FAIL;
    }
}

static void disconnect_from_wifi_and_restore_ap(void)
{
    ESP_LOGI(TAG, "Disconnecting from WiFi and restoring AP-only mode");

    // Stop WiFi
    esp_wifi_stop();

    // Set mode back to AP only
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    // Restart WiFi in AP mode
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi restored to AP-only mode");
}

esp_err_t sync_time_from_sntp(void)
{
    ESP_LOGI(TAG, "Starting SNTP time synchronization...");

    // Initialize SNTP
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, SNTP_SERVER_1);
    esp_sntp_setservername(1, SNTP_SERVER_2);
    esp_sntp_setservername(2, SNTP_SERVER_3);

    // Set callback for time sync notification
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);

    esp_sntp_init();

    // Wait for time sync (with timeout)
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           TIME_SYNC_DONE_BIT,
                                           pdTRUE,
                                           pdFALSE,
                                           pdMS_TO_TICKS(TIME_SYNC_TIMEOUT_SECONDS * 1000));

    esp_sntp_stop();

    if (bits & TIME_SYNC_DONE_BIT)
    {
        ESP_LOGI(TAG, "Time synchronization successful");
        return ESP_OK;
    }
    else
    {
        ESP_LOGW(TAG, "Time synchronization failed or timed out");
        return ESP_FAIL;
    }
}

static void time_sync_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Time sync task started - will sync every %d minutes", TIME_SYNC_INTERVAL_MINUTES);

    // Initial sync attempt after 30 seconds
    vTaskDelay(pdMS_TO_TICKS(30000));

    while (1)
    {
        ESP_LOGI(TAG, "Starting periodic time synchronization...");

        // Check current time status
        if (is_time_valid())
        {
            char current_time[64];
            get_current_time_string(current_time, sizeof(current_time));
            ESP_LOGI(TAG, "Current time before sync: %s", current_time);
        }
        else
        {
            ESP_LOGI(TAG, "System time is not valid, attempting first sync...");
        }

        // Try to connect to WiFi
        if (connect_to_wifi_for_sync() == ESP_OK)
        {
            // Perform time sync
            if (sync_time_from_sntp() == ESP_OK)
            {
                char time_str[64];
                get_current_time_string(time_str, sizeof(time_str));
                ESP_LOGI(TAG, "Time synchronized successfully: %s", time_str);
            }
            else
            {
                ESP_LOGW(TAG, "Time synchronization failed");
            }
        }
        else
        {
            ESP_LOGW(TAG, "Failed to connect to WiFi for time sync");
        }

        // Always restore AP-only mode
        disconnect_from_wifi_and_restore_ap();

        // Wait for next sync interval
        ESP_LOGI(TAG, "Next time sync in %d minutes", TIME_SYNC_INTERVAL_MINUTES);
        vTaskDelay(pdMS_TO_TICKS(TIME_SYNC_INTERVAL_MS));
    }
}

void time_sync_init(void)
{
    // Create event group
    wifi_event_group = xEventGroupCreate();

    // Create default WiFi STA interface (needed for STA+AP mode)
    esp_netif_create_default_wifi_sta();

    // Register event handlers for STA mode
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_sta_event_handler,
                                                        NULL,
                                                        NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_sta_event_handler,
                                                        NULL,
                                                        NULL));

    ESP_LOGI(TAG, "Time sync initialization complete");
}

void time_sync_start(void)
{
    if (time_sync_task_handle == NULL)
    {
        xTaskCreate(time_sync_task, "time_sync_task", 4096, NULL, 5, &time_sync_task_handle);
        ESP_LOGI(TAG, "Time sync task started");
    }
}

bool is_time_synchronized(void)
{
    return time_synchronized;
}

void get_current_time_string(char *buffer, size_t buffer_size)
{
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", &timeinfo);
}

time_t get_current_timestamp(void)
{
    time_t now;
    time(&now);
    return now;
}

bool is_time_valid(void)
{
    time_t now;
    time(&now);

    // Check if time is after 2020 (reasonable assumption)
    // Timestamp for 2020-01-01 00:00:00 UTC is 1577836800
    return now > 1577836800;
}

esp_err_t trigger_manual_time_sync(void)
{
    ESP_LOGI(TAG, "Manual time sync triggered");

    esp_err_t ret = ESP_FAIL;

    // Try to connect to WiFi
    if (connect_to_wifi_for_sync() == ESP_OK)
    {
        // Perform time sync
        ret = sync_time_from_sntp();
        if (ret == ESP_OK)
        {
            char time_str[64];
            get_current_time_string(time_str, sizeof(time_str));
            ESP_LOGI(TAG, "Manual time sync successful: %s", time_str);
        }
    }

    // Always restore AP-only mode
    disconnect_from_wifi_and_restore_ap();

    return ret;
}