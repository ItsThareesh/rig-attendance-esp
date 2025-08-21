#include <vector>
#include <system_error>
#include <array>

#include "esp_log.h"
#include "esp_err.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

#include "st25dv.hpp"
#include "ndef.hpp"
#include "hmac_token_generator.h"

static const char *TAG = "NFC";
static espp::St25dv *global_st25dv = nullptr;
static HMACTokenGenerator *global_hmac_generator = nullptr;

void configure_i2c_nfc(void)
{
    ESP_LOGI(TAG, "Configuring I2C for NFC...");

    // Configure I2C with ESP-IDF directly
    i2c_config_t i2c_config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = 21,
        .scl_io_num = 22,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = {
            .clk_speed = 400000,
        }};

    esp_err_t err = i2c_param_config(I2C_NUM_1, &i2c_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "i2c_param_config failed: %s", esp_err_to_name(err));
        return;
    }

    err = i2c_driver_install(I2C_NUM_1, I2C_MODE_MASTER, 0, 0, 0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(TAG, "i2c_driver_install failed: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "I2C configured successfully for NFC");
}

void write_nfc(void)
{
    ESP_LOGI(TAG, "Writing NDEF records to NFC...");

    if (!global_st25dv || !global_hmac_generator)
    {
        ESP_LOGE(TAG, "NFC or HMAC generator not initialized");
        return;
    }

    std::error_code ec;

    // Generate fresh token for attendance
    std::string token = global_hmac_generator->generateToken(1); // accessMethod = 1 for NFC

    // Create URL with token
    std::string url = "webapp--rig-attendance-app.asia-east1.hosted.app/scan?" + token;

    // Create NDEF records
    std::vector<espp::Ndef> records;
    records.emplace_back(espp::Ndef::make_text("RIG Attendance System"));
    records.emplace_back(espp::Ndef::make_uri(url, espp::Ndef::Uic::HTTPS));

    // Write NDEF records
    global_st25dv->set_records(records, ec);
    if (ec)
    {
        ESP_LOGE(TAG, "Failed to write NDEF records: %s", ec.message().c_str());
        return;
    }

    ESP_LOGI(TAG, "NDEF records written successfully with token");
}

// Function to detect NFC tap and update token
void check_nfc_tap(void)
{
    if (!global_st25dv)
        return;

    std::error_code ec;

    // Try to read a small amount of data to check if NFC is active
    // ST25DV will respond differently when being accessed by a reader
    static uint32_t last_check_time = 0;
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // Only check every few seconds to avoid overwhelming the system
    if (current_time - last_check_time < 5000)
    {
        return;
    }
    last_check_time = current_time;

    // Try to read from a specific system register that might indicate activity
    std::array<uint8_t, 1> test_data;
    global_st25dv->read(test_data.data(), 1, ec);
}

// Timer callback for periodic NFC updates
static void nfc_timer_callback(TimerHandle_t xTimer)
{
    ESP_LOGI(TAG, "Periodic NFC update (every 30 seconds)");
    write_nfc();
}

// Timer callback for NFC tap checking
static void nfc_tap_check_callback(TimerHandle_t xTimer)
{
    check_nfc_tap();
}

// Task to initialize NFC and start timers
void start_nfc_task(void)
{
    ESP_LOGI(TAG, "Starting NFC task...");

    // Configure I2C first
    configure_i2c_nfc();

    // Create write/read functions for espp::St25dv using native ESP-IDF
    auto write_fn = [](uint8_t device_address, const uint8_t *data, size_t length) -> bool
    {
        esp_err_t ret = i2c_master_write_to_device(I2C_NUM_1, device_address, data, length, pdMS_TO_TICKS(100));
        return ret == ESP_OK;
    };

    auto read_fn = [](uint8_t device_address, uint8_t *data, size_t length) -> bool
    {
        esp_err_t ret = i2c_master_read_from_device(I2C_NUM_1, device_address, data, length, pdMS_TO_TICKS(100));
        return ret == ESP_OK;
    };

    // Create St25dv configuration
    espp::St25dv::Config st25dv_config;
    st25dv_config.write = write_fn;
    st25dv_config.read = read_fn;
    st25dv_config.log_level = espp::Logger::Verbosity::INFO;

    // Initialize St25dv
    static espp::St25dv st25dv(st25dv_config);
    global_st25dv = &st25dv;

    // Initialize HMAC generator
    static HMACTokenGenerator hmac_generator("your-very-secret-key");
    global_hmac_generator = &hmac_generator;

    // Write initial NDEF records
    write_nfc();

    // Create timer for periodic updates every 30 seconds
    TimerHandle_t nfc_timer = xTimerCreate(
        "nfc_update_timer",
        pdMS_TO_TICKS(30000), // 30 seconds
        pdTRUE,               // Auto-reload
        (void *)0,            // Timer ID
        nfc_timer_callback    // Callback function
    );

    // Create timer for NFC tap checking every 2 seconds
    TimerHandle_t tap_check_timer = xTimerCreate(
        "nfc_tap_timer",
        pdMS_TO_TICKS(2000),   // 2 seconds
        pdTRUE,                // Auto-reload
        (void *)1,             // Timer ID
        nfc_tap_check_callback // Callback function
    );

    if (nfc_timer != NULL)
    {
        xTimerStart(nfc_timer, 0);
        ESP_LOGI(TAG, "NFC periodic update timer started (30s interval)");
    }
    else
        ESP_LOGE(TAG, "Failed to create NFC update timer");

    if (tap_check_timer != NULL)
    {
        xTimerStart(tap_check_timer, 0);
        ESP_LOGI(TAG, "NFC tap check timer started (2s interval)");
    }
    else
        ESP_LOGE(TAG, "Failed to create NFC tap check timer");
}
