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

#include "hmac_token_generator.h"
#include "nfc.h"
#include "time_sync.h"
#include "ndef.hpp"

static const char *TAG = "NFC";
static espp::St25dv *global_st25dv = nullptr;
static HMACTokenGenerator *global_hmac_generator = nullptr;

void configure_i2c_nfc(void)
{
    ESP_LOGI(TAG, "Configuring I2C for NFC...");

    // Configure I2C with ESP-IDF directly
    i2c_config_t i2c_config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = NFC_SDA_GPIO,
        .scl_io_num = NFC_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = {
            .clk_speed = 1000000, // Two-wire I2C serial interface supports 1 MHz protocol (mentioned in datasheet)
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

std::vector<uint8_t> serialize_ndef_records(std::vector<espp::Ndef> &records)
{
    std::vector<uint8_t> record_data;
    size_t total_size = 0;

    for (auto &record : records)
        total_size += record.get_size();

    record_data.reserve(total_size);

    for (size_t i = 0; i < records.size(); i++)
    {
        bool message_begin = (i == 0);
        bool message_end = (i == records.size() - 1);
        auto serialized_record = records[i].serialize(message_begin, message_end);
        record_data.insert(record_data.end(), serialized_record.begin(), serialized_record.end());
    }

    return record_data;
}

void write_nfc_ftm(TimerHandle_t xTimer)
{
    // Ensure system time has been synchronized at least once and
    // NFC and HMAC generator are initialized
    if (!is_time_valid() || !global_st25dv || !global_hmac_generator)
    {
        ESP_LOGW(TAG, "NFC write skipped - not ready");
        return;
    }

    ESP_LOGI(TAG, "Periodic NFC update (every 3 seconds)");

    std::error_code ec;
    static char url_buffer[512];

    // Generate fresh token for attendance
    std::string token = global_hmac_generator->generateToken(1); // accessMethod = 1 for NFC

    // Create URL with token
    snprintf(url_buffer, sizeof(url_buffer),
             "webapp--rig-attendance-app.asia-east1.hosted.app/scan?%s",
             token.c_str());

    // Create NDEF records
    std::vector<espp::Ndef> records;
    records.emplace_back(espp::Ndef::make_uri(url_buffer, espp::Ndef::Uic::HTTPS));

    // Serialize NDEF records for writing into FTM mailbox
    std::vector<uint8_t> serialized_records = serialize_ndef_records(records);

    // Write NDEF records to FTM mailbox
    global_st25dv->transfer(serialized_records.data(), serialized_records.size(), ec);
    if (ec)
    {
        ESP_LOGE(TAG, "Failed to write NDEF records: %s", ec.message().c_str());
        return;
    }

    ESP_LOGI(TAG, "NDEF records written successfully to FTM mailbox");
}

// Function to detect NFC tap and update token
// void check_nfc_tap(TimerHandle_t xTimer)
// {
//     if (!global_st25dv)
//         return;
//
//     std::error_code ec;
//
//     // Try to read a small amount of data to check if NFC is active
//     // ST25DV will respond differently when being accessed by a reader
//     static uint32_t last_check_time = 0;
//     uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
//
//     // Only check every few seconds to avoid overwhelming the system
//     if (current_time - last_check_time < 5000)
//     {
//         return;
//     }
//     last_check_time = current_time;
//
//     // Try to read from a specific system register that might indicate activity
//     std::array<uint8_t, 1> test_data;
//     global_st25dv->read(test_data.data(), 1, ec);
// }

void write_static_eeprom_record()
{
    configure_i2c_nfc();

    if (!global_st25dv)
        return;

    std::error_code ec;

    // Create static record
    std::vector<espp::Ndef> static_record;
    static_record.emplace_back(espp::Ndef::make_text("RIG Attendance System"));

    // Write to EEPROM
    global_st25dv->set_records(static_record, ec);
    if (ec)
    {
        ESP_LOGE(TAG, "Failed to write static EEPROM record: %s", ec.message().c_str());
        return;
    }

    ESP_LOGI(TAG, "Static EEPROM record written successfully");
}

// Task to initialize NFC and start timers
void start_nfc_task(HMACTokenGenerator *hmac_generator)
{
    ESP_LOGI(TAG, "Starting NFC task...");

    // // Create write/read functions for espp::St25dv using native ESP-IDF
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
    espp::St25dv::Config st25dv_config({.write = write_fn,
                                        .read = read_fn,
                                        .log_level = espp::Logger::Verbosity::INFO});

    // Initialize St25dv
    static espp::St25dv st25dv(st25dv_config);
    global_st25dv = &st25dv;

    // Write static record to EEPROM once on startup
    write_static_eeprom_record();

    // Start FTM
    std::error_code ec;
    global_st25dv->start_fast_transfer_mode(ec);
    if (ec)
    {
        ESP_LOGE(TAG, "Failed to initialize NFC: %s", ec.message().c_str());
        return;
    }

    // Initialize Global HMAC generator to passed parameter
    global_hmac_generator = hmac_generator;

    // Create timer for periodic updates every 3 seconds
    TimerHandle_t nfc_timer = xTimerCreate(
        "write_nfc_timer",
        pdMS_TO_TICKS(NFC_UPDATE_INTERVAL_MS), // 3 seconds
        pdTRUE,                                // Auto-reload
        (void *)0,                             // Timer ID
        write_nfc_ftm                          // Callback function
    );

    // Checks for whether timer is working properly
    if (nfc_timer != NULL)
    {
        xTimerStart(nfc_timer, 0);
        ESP_LOGI(TAG, "NFC periodic update timer started (30s interval)");
    }
    else
        ESP_LOGE(TAG, "Failed to create NFC update timer");
}
