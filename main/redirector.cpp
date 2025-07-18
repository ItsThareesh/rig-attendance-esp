#include "esp_log.h"
#include "esp_http_server.h"
#include "hmac_token_generator.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "CaptivePortal";
static HMACTokenGenerator *global_hmac_generator = nullptr;

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

    // Create a mutable copy of the HTML
    char *html_content = (char *)malloc(content_length + 300); // Extra space for longer URLs
    if (html_content == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for HTML content");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    memcpy(html_content, root_start, content_length);
    html_content[content_length] = '\0';

    // Generate dynamic link with token
    char dynamic_link[256];
    if (global_hmac_generator != nullptr)
    {
        // Generate a token for current timestamp
        std::string token = global_hmac_generator->generateToken("web_access", 0);

        // Create the dynamic link
        snprintf(dynamic_link, sizeof(dynamic_link),
                 "https://example.com/checkin?token=%s",
                 token.c_str());

        ESP_LOGI(TAG, "Generated link : %s", dynamic_link);
    }
    else
    {
        // Fallback if HMAC generator is not available
        strcpy(dynamic_link, "https://example.com/checkin?token=unavailable");
        ESP_LOGW(TAG, "HMAC generator not available, using fallback link");
    }

    // Replace template placeholder with dynamic link
    char *placeholder = strstr(html_content, "{{DYNAMIC_LINK}}");
    if (placeholder != NULL)
    {
        // Calculate positions and lengths
        size_t before_len = placeholder - html_content;
        size_t placeholder_len = strlen("{{DYNAMIC_LINK}}");
        size_t after_len = strlen(placeholder + placeholder_len);
        size_t dynamic_link_len = strlen(dynamic_link);

        // Check if we have enough space
        size_t new_total_len = before_len + dynamic_link_len + after_len;
        if (new_total_len < content_length + 300)
        {
            // Shift content to make room for new link
            memmove(placeholder + dynamic_link_len,
                    placeholder + placeholder_len,
                    after_len + 1); // +1 for null terminator

            // Insert new link
            memcpy(placeholder, dynamic_link, dynamic_link_len);

            content_length = new_total_len;
        }
        else
        {
            ESP_LOGE(TAG, "Not enough space to replace placeholder");
        }
    }
    else
    {
        ESP_LOGW(TAG, "Template placeholder not found in HTML");
    }

    ESP_LOGI(TAG, "Serve Root");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_content, content_length);

    free(html_content);
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

// Set the HMAC generator instance
void set_hmac_generator(HMACTokenGenerator *generator)
{
    global_hmac_generator = generator;
    ESP_LOGI(TAG, "HMAC generator set successfully");
}