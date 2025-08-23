file(GLOB_RECURSE SOURCES "*.cpp" "*.c")
idf_component_register(
    SRCS ${SOURCES}
    PRIV_REQUIRES dns_server esp_http_server esp_wifi nvs_flash mbedtls esp_timer driver
    INCLUDE_DIRS "." "../include"
    EMBED_FILES root.html
)