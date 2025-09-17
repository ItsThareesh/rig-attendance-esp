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
#include "freertos/queue.h"

#include "nfc.h"
#include "time_sync.h"
#include "ndef.hpp"

static const char *TAG = "NFC";

static espp::St25dv *global_st25dv = nullptr;
static HMACTokenGenerator *global_hmac_generator = nullptr;
static QueueHandle_t gpo_evt_queue = NULL;
static espp::Ndef record = espp::Ndef::make_uri(
    "webapp--rig-attendance-app.asia-east1.hosted.app", espp::Ndef::Uic::HTTPS);

static void nfc_gpo_isr(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(gpo_evt_queue, &gpio_num, NULL);
}

esp_err_t nfc_gpio_init()
{
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << NFC_GPO_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE, // Rising edge = field detected
    };

    gpio_config(&io_conf);

    gpio_install_isr_service(0); // Default ISR service
    gpio_isr_handler_add(gpio_num_t(NFC_GPO_GPIO), nfc_gpo_isr, (void *)NFC_GPO_GPIO);

    return ESP_OK;
}

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
            .clk_speed = 100000, // Two-wire I2C serial interface supports 1 MHz protocol (mentioned in datasheet)
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

void generate_nfc_url(TimerHandle_t xTimer)
{
    // Ensure system time has been synchronized at least once and
    // NFC and HMAC generator are initialized
    if (!is_time_valid() || !global_st25dv || !global_hmac_generator)
    {
        ESP_LOGW(TAG, "URL Generate skipped - not ready");
        return;
    }

    std::error_code ec;
    static char url_buffer[512];

    // Generate fresh token for attendance
    std::string token = global_hmac_generator->generateToken(1); // accessMethod = 1 for NFC

    // Create URL with token
    snprintf(url_buffer, sizeof(url_buffer),
             "webapp--rig-attendance-app.asia-east1.hosted.app/scan?%s",
             token.c_str());

    // Create NDEF records
    record = espp::Ndef::make_uri(url_buffer, espp::Ndef::Uic::HTTPS);
}

void gpo_event_task(void *pvParameters)
{
    uint32_t gpio_num;
    ESP_LOGI(TAG, "GPO task started - waiting for phone detection");
    static TickType_t last_event_tick = 0;

    while (1)
    {
        if (xQueueReceive(gpo_evt_queue, &gpio_num, portMAX_DELAY) == pdTRUE)
        {
            TickType_t now = xTaskGetTickCount();

            // Too soon, ignore duplicate
            if (now - last_event_tick < pdMS_TO_TICKS(NFC_UPDATE_INTERVAL_MS))
                continue;

            last_event_tick = now;

            ESP_LOGI(TAG, "Phone detected! Writing to EEPROM now...");

            std::error_code ec;
            global_st25dv->set_record(record, ec);
            if (ec)
                ESP_LOGE(TAG, "GPO: Failed to write EEPROM record: %s", ec.message().c_str());
            else
                ESP_LOGI(TAG, "GPO: EEPROM record written successfully");
        }
    }
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
        esp_err_t ret = i2c_master_write_to_device(I2C_NUM_1, device_address, data, length, pdMS_TO_TICKS(500));
        if (ret != ESP_OK)
        {
            ESP_LOGW("NFC", "Write failed to 0x%02X: %s", device_address, esp_err_to_name(ret));
            return false;
        }
        return true;
    };

    auto read_fn = [](uint8_t device_address, uint8_t *data, size_t length) -> bool
    {
        esp_err_t ret = i2c_master_read_from_device(I2C_NUM_1, device_address, data, length, pdMS_TO_TICKS(500));
        if (ret != ESP_OK)
        {
            ESP_LOGW("NFC", "Read failed from 0x%02X: %s", device_address, esp_err_to_name(ret));
            return false;
        }
        return true;
    };

    // Create St25dv configuration
    espp::St25dv::Config st25dv_config;
    st25dv_config.write = write_fn;
    st25dv_config.read = read_fn;
    st25dv_config.log_level = espp::Logger::Verbosity::INFO; // More verbose logging

    // Initialize St25dv
    static espp::St25dv st25dv(st25dv_config);
    global_st25dv = &st25dv;

    // Initialize Global HMAC generator to passed parameter
    global_hmac_generator = hmac_generator;

    // Create queue for GPO events (RF field detection)
    gpo_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    if (gpo_evt_queue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create GPO event queue");
        return;
    }

    // Configure GPO interrupt for RF field detection
    if (nfc_gpio_init() != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure GPO interrupt");
        return;
    }

    // Create GPO event task for immediate RF field response
    TaskHandle_t gpo_task_handle = NULL;
    BaseType_t task_created = xTaskCreate(
        gpo_event_task,
        "gpo_rf_task",
        4096, // Stack size
        NULL, // Task parameters
        5,    // Priority (higher than timer task)
        &gpo_task_handle);

    if (task_created != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create GPO RF field task");
        return;
    }

    // Create timer for periodic updates every 5 seconds
    TimerHandle_t nfc_timer = xTimerCreate(
        "generate_nfc_url_timer",
        pdMS_TO_TICKS(NFC_UPDATE_INTERVAL_MS), // 5 seconds
        pdTRUE,                                // Auto-reload
        (void *)0,                             // Timer ID
        generate_nfc_url                       // Callback function
    );

    if (nfc_timer != NULL)
    {
        xTimerStart(nfc_timer, 0);
        ESP_LOGI(TAG, "NFC periodic update timer started (%ds interval)", NFC_UPDATE_INTERVAL_MS / 1000);
    }
    else
        ESP_LOGE(TAG, "Failed to create NFC update timer");
}