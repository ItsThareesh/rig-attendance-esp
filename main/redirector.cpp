#include "esp_log.h"
#include "esp_http_server.h"

// #define REDIRECT_URL "https://www.google.com"

static const char *TAG = "CaptivePortal";

extern const char root_start[] asm("_binary_root_html_start");
extern const char root_end[] asm("_binary_root_html_end");
// extern const char success_start[] asm("_binary_success_html_start");
// extern const char success_end[] asm("_binary_success_html_end");

// Handler to serve the Main Captive Portal Page
static esp_err_t root_get_handler(httpd_req_t *req)
{
    const uint32_t root_len = root_end - root_start;

    ESP_LOGI(TAG, "Serve root");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, root_start, root_len);

    return ESP_OK;
}

// static esp_err_t continue_post_handler(httpd_req_t *req)
// {
//     ESP_LOGI(TAG, "User authentication successful. Serving success page.");

//     const uint32_t success_len = success_end - success_start;

//     // Respond with 200 OK and the content of success.html
//     httpd_resp_set_status(req, "200 OK");
//     httpd_resp_set_type(req, "text/html");
//     httpd_resp_send(req, success_start, success_len);

//     return ESP_OK;
// }

// This handler redirects any other request to the root page.
esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    // Set status
    httpd_resp_set_status(req, "302 Temporary Redirect");
    // Redirect to the "/" root directory
    httpd_resp_set_hdr(req, "Location", "/");
    // iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
    httpd_resp_send(req, "Redirecting to the Captive Portal", HTTPD_RESP_USE_STRLEN);

    ESP_LOGI(TAG, "Redirecting to root");
    return ESP_OK;
}

// URI definition for the root page
static const httpd_uri_t root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_get_handler,
};

// URI definition for the continue button action
// static const httpd_uri_t continue_uri = {
//     .uri = "/continue",
//     .method = HTTP_POST,
//     .handler = continue_post_handler,
// };

httpd_handle_t start_http_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 1;
    httpd_handle_t server = NULL;

    ESP_LOGI(TAG, "Starting Server on Port: '%d'", config.server_port);

    if (httpd_start(&server, &config) == ESP_OK)
    {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &root);
        // httpd_register_uri_handler(server, &continue_uri);
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);
    }

    return server;
}