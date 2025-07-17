#include "esp_log.h"
#include "esp_http_server.h"

static const char *TAG = "CaptivePortal";

extern const char root_start[] asm("_binary_root_html_start");
extern const char root_end[] asm("_binary_root_html_end");

// Function to read HTML file from filesystem
// static char *read_html_file(const char *filename)
// {
//     FILE *file = fopen(filename, "r");

//     // Get file size
//     fseek(file, 0, SEEK_END);
//     long file_size = ftell(file);
//     fseek(file, 0, SEEK_SET);

//     // Allocate memory for content
//     char *content = (char *)malloc(file_size + 1);
//     if (content == NULL)
//     {
//         ESP_LOGE(TAG, "Failed to allocate memory for file content");
//         fclose(file);
//         return NULL;
//     }

//     // Read file content
//     fread(content, 1, file_size, file);
//     content[file_size] = '\0';

//     fclose(file);
//     return content;
// }

// Handler to serve the Main Captive Portal Page
static esp_err_t root_get_handler(httpd_req_t *req)
{
    uint32_t content_length = root_end - root_start;

    ESP_LOGI(TAG, "Serve Root");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, root_start, content_length);

    // free(html_content);
    return ESP_OK;
}

// This handler redirects any other request to the root page.
esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    // Set status
    httpd_resp_set_status(req, "302 Temporary Redirect");
    // Redirect to the "/" root directory
    httpd_resp_set_hdr(req, "Location", "/");
    // iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
    httpd_resp_send(req, "Redirect", HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

// URI definition for the root page
static const httpd_uri_t root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_get_handler,
    .user_ctx = NULL,
};

httpd_handle_t start_websever(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.max_open_sockets = 3;
    httpd_handle_t server = NULL;

    ESP_LOGI(TAG, "Starting Server on Port: '%d'", config.server_port);

    if (httpd_start(&server, &config) == ESP_OK)
    {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &root);
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);
    }

    return server;
}