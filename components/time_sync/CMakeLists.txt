idf_component_register(
    SRCS "time_sync.cpp"
    PRIV_REQUIRES wifi_connect esp_wifi
    INCLUDE_DIRS "include"
)
