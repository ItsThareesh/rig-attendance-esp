idf_component_register(
    SRCS "wifi_ap_sta.cpp" "redirector.cpp"
    PRIV_REQUIRES hmac_token_generator mbedtls time_sync esp_wifi esp_http_server
    INCLUDE_DIRS "include"
    EMBED_FILES root.html
)