#include <sys/time.h>
#include <string.h>

#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "esp_event.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "time_sync.h"
#include "wifi_ap_sta.h"

static const char *TAG = "TimeSync";

// Time sync interval (configurable)
#define TIME_SYNC_INTERVAL_MS (TIME_SYNC_INTERVAL_MINUTES * 60 * 1000)

// Event group bits
#define TIME_SYNC_DONE_BIT BIT0

static EventGroupHandle_t time_sync_event_group;
static TaskHandle_t periodic_time_sync_task_handle = NULL;
static TickType_t last_sync_attempt = 0;

static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Time synchronized via SNTP");
    xEventGroupSetBits(time_sync_event_group, TIME_SYNC_DONE_BIT);
}

void get_current_time_string(char *buffer, size_t buffer_size)
{
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", &timeinfo);
}

bool is_time_valid(void)
{
    time_t now;
    time(&now);

    // Timestamp for Wednesday, January 1, 2025 12:00:00 AM UTC
    return now > 1735689600;
}

esp_err_t sync_time_from_sntp(void)
{
    if (!is_sta_connected())
    {
        ESP_LOGW(TAG, "WiFi STA not connected, cannot sync time");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Starting SNTP time synchronization...");

    // Update last sync attempt time
    last_sync_attempt = xTaskGetTickCount();

    // Clear event bits
    xEventGroupClearBits(time_sync_event_group, TIME_SYNC_DONE_BIT);

    // Initialize SNTP
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, SNTP_SERVER_1);
    esp_sntp_setservername(1, SNTP_SERVER_2);
    esp_sntp_setservername(2, SNTP_SERVER_3);

    // Set callback for time sync notification
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);

    esp_sntp_init();

    // Wait for time sync (with timeout)
    EventBits_t bits = xEventGroupWaitBits(time_sync_event_group,
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

static void periodic_time_sync_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Time sync task started - Will sync every %d minutes", TIME_SYNC_INTERVAL_MINUTES);

    while (1)
    {
        ESP_LOGI(TAG, "Starting periodic time synchronization");

        // Check current time status
        if (is_time_valid())
        {
            char current_time[64];
            get_current_time_string(current_time, sizeof(current_time));
            ESP_LOGI(TAG, "Current time before sync: %s", current_time);
        }
        else
        {
            ESP_LOGI(TAG, "System time is invalid, Attempting first sync");
        }

        // Check if WiFi STA is connected
        if (is_sta_connected())
        {
            // Perform time sync
            if (sync_time_from_sntp() == ESP_OK)
            {
                char time_str[64];
                get_current_time_string(time_str, sizeof(time_str));
                ESP_LOGI(TAG, "Time after sync: %s", time_str);
            }
        }
        else
        {
            ESP_LOGW(TAG, "WiFi STA not connected, skipping time sync");
        }

        // Wait for next sync interval
        ESP_LOGI(TAG, "Next time sync in %d minutes", TIME_SYNC_INTERVAL_MINUTES);
        vTaskDelay(pdMS_TO_TICKS(TIME_SYNC_INTERVAL_MS));
    }
}

void time_sync_init(void)
{
    // Set timezone to IST (UTC+5:30) according to POSIX format
    setenv("TZ", "IST-5:30", 1);
    tzset();

    // Create event group
    time_sync_event_group = xEventGroupCreate();

    ESP_LOGI(TAG, "Time sync initialization complete");

    if (periodic_time_sync_task_handle == NULL)
        xTaskCreate(periodic_time_sync_task, "periodic_time_sync_task", 4096, NULL, 5, &periodic_time_sync_task_handle);
}

esp_err_t trigger_manual_time_sync(void)
{
    ESP_LOGI(TAG, "Manual time sync triggered");

    esp_err_t ret = ESP_FAIL;

    // Check if WiFi STA is connected
    if (is_sta_connected())
    {
        // Perform time sync
        ret = sync_time_from_sntp();
        if (ret == ESP_OK)
        {
            char time_str[64];
            get_current_time_string(time_str, sizeof(time_str));
            ESP_LOGI(TAG, "Manual time sync successful: %s", time_str);
        }
        else
            ESP_LOGW(TAG, "Manual time sync failed");
    }
    else
        ESP_LOGW(TAG, "WiFi STA not connected, cannot perform manual time sync");

    return ret;
}

static void async_time_sync_task(void *pvParameters)
{
    if (trigger_manual_time_sync() == ESP_OK)
        ESP_LOGI(TAG, "Async time sync completed successfully");
    else
        ESP_LOGW(TAG, "Async time sync failed");

    // Delete this task
    vTaskDelete(NULL);
}

esp_err_t trigger_async_time_sync(void)
{
    // Create a task to perform time sync asynchronously
    BaseType_t result = xTaskCreate(async_time_sync_task, "async_time_sync", 4096, NULL, 5, NULL);

    if (result == pdPASS)
        return ESP_OK;
    else
    {
        ESP_LOGE(TAG, "Failed to create async time sync task");
        return ESP_FAIL;
    }
}