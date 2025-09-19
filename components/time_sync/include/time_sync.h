#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

// Time sync settings
#define TIME_SYNC_INTERVAL_MINUTES 10
#define WIFI_CONNECT_TIMEOUT_SECONDS 30
#define TIME_SYNC_TIMEOUT_SECONDS 30

// SNTP servers
#define SNTP_SERVER_1 "pool.ntp.org"
#define SNTP_SERVER_2 "time.nist.gov"
#define SNTP_SERVER_3 "time.google.com"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Initialize and start time synchronization task
     */
    void time_sync_init(void);

    /**
     * @brief Check if time is valid (after 2025, not default 1970 time)
     * @return true if time appears valid, false otherwise
     */
    bool is_time_valid(void);

    /**
     * @brief Trigger asynchronous time synchronization (non-blocking)
     * Creates a task to perform time sync without blocking the caller
     * @return ESP_OK if task created successfully, ESP_FAIL otherwise
     */
    esp_err_t trigger_async_time_sync(void);

#ifdef __cplusplus
}
#endif