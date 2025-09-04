#include "esp_log.h"
#include "nvs_flash.h"
#include "wifi_ap_sta.h"
#include "redirector.h"
#include "dns_server.h"
#include "hmac_token_generator.h"
#include "time_sync.h"
#include "nfc.h"

extern "C" void app_main(void)
{
    esp_log_level_set("httpd_uri", ESP_LOG_ERROR);
    esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);
    esp_log_level_set("httpd_parse", ESP_LOG_ERROR);

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize WiFi in SoftAP mode
    wifi_init_softap();

    // Initialize and start time synchronization
    time_sync_init();

    // Initialize HMAC token generator with a secret key
    HMACTokenGenerator *hmac_generator = new HMACTokenGenerator("your-very-secret-key");

    // Initialize Webserver and DNS Server
    start_webserver(hmac_generator);
    dns_server_config_t config = DNS_SERVER_CONFIG_SINGLE("*", "WIFI_AP_DEF");
    start_dns_server(&config);

    // Start NFC task for periodic updates and tap detection
    start_nfc_task(hmac_generator);
}