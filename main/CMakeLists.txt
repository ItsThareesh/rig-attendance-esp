file(GLOB_RECURSE SOURCES "*.cpp" "*.c")
idf_component_register(
    SRCS ${SOURCES}
    PRIV_REQUIRES hmac_token_generator nfc wifi_connect time_sync dns_server esp_netif nvs_flash
    INCLUDE_DIRS "." "../include"
)