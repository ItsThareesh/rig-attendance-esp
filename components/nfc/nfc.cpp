#include <vector>
#include <system_error>
#include <array>

#include "esp_log.h"
#include "esp_err.h"
#include "driver/i2c.h"
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
            .clk_speed = 400000, // Two-wire I2C serial interface supports 1 MHz protocol (mentioned in datasheet)
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

esp_err_t write_static_eeprom_record()
{
    if (!global_st25dv)
        return ESP_FAIL;

    std::error_code ec;

    // Create static record
    std::vector<espp::Ndef> records;
    records.emplace_back(espp::Ndef::make_text("RIG Attendance System 2025"));

    // Write to EEPROM
    global_st25dv->set_records(records, ec);
    if (ec)
    {
        ESP_LOGE(TAG, "Failed to write static EEPROM record: %s", ec.message().c_str());
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Static EEPROM record written successfully");
    return ESP_OK;
}

bool present_password(const uint8_t pwd[8])
{
    uint8_t buffer[2 + 17] = {0};

    // Register address (I2C_PWD)
    buffer[0] = 0x09;
    buffer[1] = 0x00;

    // Copy password
    memcpy(&buffer[2], pwd, 8);

    // Validation code
    buffer[10] = 0x09;

    // Second password
    memcpy(&buffer[11], pwd, 8);

    // Send to ST25DV
    esp_err_t ret = i2c_master_write_to_device(
        I2C_NUM_1,
        espp::St25dv::SYST_ADDRESS,
        buffer,
        sizeof(buffer),
        pdMS_TO_TICKS(200));

    if (ret != ESP_OK)
        return false;

    return true;
}

// Add this function to write to system register 0x000D (MB_MODE)
esp_err_t enable_mb_mode()
{
    uint8_t default_password[8] = {0, 0, 0, 0, 0, 0, 0, 0};

    if (!present_password(default_password))
    {
        ESP_LOGE(TAG, "Cannot enable MB_MODE without presenting password");
        return ESP_FAIL;
    }

    std::error_code ec;

    // Write to MB_MODE register (0x000D) using SYST_ADDRESS
    uint8_t write_data[3] = {
        0x00, // High byte of address 0x000D
        0x0D, // Low byte of address 0x000D
        0x01  // Enable MB_MODE (set to 1)
    };

    esp_err_t write_result = i2c_master_write_to_device(
        I2C_NUM_1,
        espp::St25dv::SYST_ADDRESS,
        write_data,
        sizeof(write_data),
        pdMS_TO_TICKS(200));

    if (write_result != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write MB_MODE register: %s", esp_err_to_name(write_result));
        return write_result;
    }

    uint8_t read_address[2] = {
        0x00, // High byte of address 0x000D
        0x0D  // Low byte of address 0x000D
    };

    uint8_t read_data[1];

    esp_err_t read_result = i2c_master_write_read_device(
        I2C_NUM_1,
        espp::St25dv::SYST_ADDRESS,
        read_address,
        sizeof(read_address),
        read_data,
        sizeof(read_data),
        pdMS_TO_TICKS(200));

    if (read_result != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read back MB_MODE register: %s", esp_err_to_name(read_result));
        return read_result;
    }

    ESP_LOGI(TAG, "MB_MODE register value: 0x%02X", read_data[0]);
    ESP_LOGI(TAG, "MB_MODE enabled successfully");
    return ESP_OK;
}

esp_err_t enable_ftm()
{
    // Enable MB_MODE in system configuration first
    ESP_LOGI(TAG, "Enabling MB_MODE system register...");
    if (enable_mb_mode() != ESP_OK)
        return ESP_FAIL;

    // Add small delay after MB_MODE
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Start FTM with detailed logging
    ESP_LOGI(TAG, "Starting start_fast_transfer_mode()");
    std::error_code ec;

    global_st25dv->start_fast_transfer_mode(ec);
    if (ec)
    {
        ESP_LOGE(TAG, "start_fast_transfer_mode() failed: %s", ec.message().c_str());
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "FTM started successfully!");
    return ESP_OK;
}

// Task to initialize NFC and start timers
void start_nfc_task(HMACTokenGenerator *hmac_generator)
{
    ESP_LOGI(TAG, "Starting NFC task...");

    // Initialize I2C for NFC
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

    // Initialize Global HMAC generator to passed parameter
    global_hmac_generator = hmac_generator;

    // Write static record to EEPROM once on startup
    if (write_static_eeprom_record() != ESP_OK)
        return;

    vTaskDelay(pdMS_TO_TICKS(2000)); // Wait 2 seconds before enabling FTM

    // Enable FTM mode
    if (enable_ftm() != ESP_OK)
        return;

    // Create timer for periodic updates every 10 seconds
    TimerHandle_t nfc_timer = xTimerCreate(
        "write_nfc_timer",
        pdMS_TO_TICKS(10000), // 10 seconds
        pdTRUE,               // Auto-reload
        (void *)0,            // Timer ID
        write_nfc_ftm         // Callback function
    );

    if (nfc_timer != NULL)
    {
        xTimerStart(nfc_timer, 0);
        ESP_LOGI(TAG, "NFC periodic update timer started (10s interval)");
    }
    else
        ESP_LOGE(TAG, "Failed to create NFC update timer");
}