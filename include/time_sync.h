#ifndef TIME_SYNC_H
#define TIME_SYNC_H

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize time synchronization
 */
void time_sync_init(void);

/**
 * @brief Start time synchronization task
 */
void time_sync_start(void);

/**
 * @brief Perform SNTP time synchronization
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t sync_time_from_sntp(void);

/**
 * @brief Check if system time is synchronized
 * @return true if time is synchronized, false otherwise
 */
bool is_time_synchronized(void);

/**
 * @brief Get current time as string
 * @param buffer Buffer to store time string
 * @param buffer_size Size of the buffer
 */
void get_current_time_string(char* buffer, size_t buffer_size);

/**
 * @brief Get current timestamp (seconds since epoch)
 * @return Current timestamp
 */
time_t get_current_timestamp(void);

/**
 * @brief Check if time is valid (not default 1970 time)
 * @return true if time appears valid, false otherwise
 */
bool is_time_valid(void);

/**
 * @brief Trigger manual time synchronization
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t trigger_manual_time_sync(void);

#ifdef __cplusplus
}
#endif

#endif // TIME_SYNC_H