idf_component_register(
    SRCS "token-generator.cpp" "redirector.cpp" "main.cpp" "wifi_softap.cpp"
    PRIV_REQUIRES dns_server esp_http_server esp_wifi nvs_flash mbedtls
    INCLUDE_DIRS "." "../include"
    EMBED_FILES root.html
)