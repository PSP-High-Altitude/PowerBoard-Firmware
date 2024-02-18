/* HTTP Restful API Server

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <fcntl.h>
#include "esp_http_server.h"
#include "esp_chip_info.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "cJSON.h"
#include "power_control.h"

extern uint8_t armed;
static httpd_handle_t server = NULL;

static const char *HTTP_TAG = "http-server";
#define INDEX_PATH "/www/index.html"
#define PSPHA_PNG_PATH "/www/pspha.png"
#define FAVICON_PATH "/www/favicon.ico"
#define JQUERY_PATH "/www/jquery.js"

// Handler for getting battery data
static esp_err_t battery_data_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateArray();

    // Flight battery info
    battery_stat_t flight_stat = get_battery(FLIGHT_BATTERY);
    cJSON *flight_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(flight_obj, "max_cap", flight_stat.max_cap);                // Maximum Capacity (mAh)
    cJSON_AddNumberToObject(flight_obj, "curr_cap", flight_stat.curr_cap);              // Current charge (mAh)
    cJSON_AddNumberToObject(flight_obj, "soc", flight_stat.soc);                        // State of charge (decimal %)
    cJSON_AddBoolToObject(flight_obj, "charging", flight_stat.charging);                // Charging?
    cJSON_AddNumberToObject(flight_obj, "charge_cycles", flight_stat.charge_cycles);    // Maximum Capacity (mAh)
    cJSON_AddNumberToObject(flight_obj, "age", flight_stat.battery_age);                // Current charge (mAh)
    cJSON_AddNumberToObject(flight_obj, "ttf", flight_stat.ttf_min);                    // State of charge (decimal %)
    cJSON_AddNumberToObject(flight_obj, "current", flight_stat.current_mah);            // Current (mAh)
    cJSON_AddNumberToObject(flight_obj, "voltage", flight_stat.batt_voltage);           // Voltage (V)
    cJSON_AddNumberToObject(flight_obj, "tte", flight_stat.tte_min);                    // Charging?
    cJSON_AddItemToArray(root, flight_obj);

    // Pyro battery info
    battery_stat_t pyro_stat = get_battery(PYRO_BATTERY);
    cJSON *pyro_obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(pyro_obj, "max_cap", pyro_stat.max_cap);                    // Maximum Capacity (mAh)
    cJSON_AddNumberToObject(pyro_obj, "curr_cap", pyro_stat.curr_cap);                  // Current charge (mAh)
    cJSON_AddNumberToObject(pyro_obj, "soc", pyro_stat.soc);                            // State of charge (decimal %)
    cJSON_AddBoolToObject(pyro_obj, "charging", pyro_stat.charging);                    // Charging?
    cJSON_AddNumberToObject(pyro_obj, "charge_cycles", pyro_stat.charge_cycles);        // Maximum Capacity (mAh)
    cJSON_AddNumberToObject(pyro_obj, "age", pyro_stat.battery_age);                    // Current charge (mAh)
    cJSON_AddNumberToObject(pyro_obj, "ttf", pyro_stat.ttf_min);                        // State of charge (decimal %)
    cJSON_AddNumberToObject(pyro_obj, "current", pyro_stat.current_mah);                // Current (mAh)
    cJSON_AddNumberToObject(pyro_obj, "voltage", pyro_stat.batt_voltage);               // Voltage (V)
    cJSON_AddNumberToObject(pyro_obj, "tte", pyro_stat.tte_min);                        // Charging?
    cJSON_AddItemToArray(root, pyro_obj);

    const char *bat_info = cJSON_Print(root);
    httpd_resp_sendstr(req, bat_info);
    free((void *)bat_info);
    cJSON_Delete(root);
    return ESP_OK;
}

// Handler for GETting arm/disarm status
static esp_err_t arm_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "armed", armed);

    const char *resp = cJSON_Print(root);
    httpd_resp_sendstr(req, resp);
    free((void *)resp);
    cJSON_Delete(root);

    return ESP_OK;
}

// Handler for arming/disarming
static esp_err_t arm_post_handler(httpd_req_t *req)
{
    int total_len = req->content_len;
    int cur_len = 0;
    char buf[total_len + 1];
    int received = 0;
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    if(armed)
    {
        set_disarmed();
    }
    else
    {
        set_armed();
    }
    
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "armed", armed);

    const char *resp = cJSON_Print(root);
    httpd_resp_sendstr(req, resp);
    free((void *)resp);
    cJSON_Delete(root);

    return ESP_OK;
}

// Handler for GETting index page
static esp_err_t index_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");

    FILE *file = fopen(INDEX_PATH, "r");
    if(file == NULL) {
        return ESP_FAIL;
    }
    char resp[4096];
    while(!feof(file))
    {
        uint16_t len = fread(resp, 1, 4096, file);
        if(httpd_resp_send_chunk(req, resp, len) != ESP_OK)
        {
            fclose(file);
            return ESP_FAIL;
        }
    }
    httpd_resp_send_chunk(req, NULL, 0);
    fclose(file);
    return ESP_OK;
}

// Handler for GETting pspha.png
static esp_err_t pspha_png_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "image/png");

    FILE *file = fopen(PSPHA_PNG_PATH, "r");
    if(file == NULL) {
        return ESP_FAIL;
    }
    char resp[4096];
    while(!feof(file))
    {
        uint16_t len = fread(resp, 1, 4096, file);
        if(httpd_resp_send_chunk(req, resp, len) != ESP_OK)
        {
            fclose(file);
            return ESP_FAIL;
        }
    }
    httpd_resp_send_chunk(req, NULL, 0);
    fclose(file);
    return ESP_OK;
}

// Handler for GETting favicon.ico
static esp_err_t favicon_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "image/x-icon");

    FILE *file = fopen(FAVICON_PATH, "r");
    if(file == NULL) {
        return ESP_FAIL;
    }
    char resp[4096];
    while(!feof(file))
    {
        uint16_t len = fread(resp, 1, 4096, file);
        if(httpd_resp_send_chunk(req, resp, len) != ESP_OK)
        {
            fclose(file);
            return ESP_FAIL;
        }
    }
    httpd_resp_send_chunk(req, NULL, 0);
    fclose(file);
    return ESP_OK;
}

// Handler for GETting favicon.ico
static esp_err_t jquery_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/javascript");

    FILE *file = fopen(JQUERY_PATH, "r");
    if(file == NULL) {
        return ESP_FAIL;
    }
    char resp[4096];
    while(!feof(file))
    {
        uint16_t len = fread(resp, 1, 4096, file);
        if(httpd_resp_send_chunk(req, resp, len) != ESP_OK)
        {
            fclose(file);
            return ESP_FAIL;
        }
    }
    httpd_resp_send_chunk(req, NULL, 0);
    fclose(file);
    return ESP_OK;
}

esp_err_t start_http_server()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(HTTP_TAG, "Starting HTTP Server");
    if(httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(HTTP_TAG, "Start server failed");
        return ESP_FAIL;
    }

    /* URI handler for fetching battery status */
    httpd_uri_t battery_data_get_uri = {
        .uri = "/battery",
        .method = HTTP_GET,
        .handler = battery_data_get_handler,
    };
    httpd_register_uri_handler(server, &battery_data_get_uri);

    /* URI handler for arming status */
    httpd_uri_t arm_get_uri = {
        .uri = "/arm",
        .method = HTTP_GET,
        .handler = arm_get_handler,
    };
    httpd_register_uri_handler(server, &arm_get_uri);

    /* URI handler for arming control */
    httpd_uri_t arm_post_uri = {
        .uri = "/arm",
        .method = HTTP_POST,
        .handler = arm_post_handler,
    };
    httpd_register_uri_handler(server, &arm_post_uri);

    /* URI handler for fetching index page */
    httpd_uri_t index_get_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = index_get_handler,
    };
    httpd_register_uri_handler(server, &index_get_uri);

    /* URI handler for fetching pspha.png */
    httpd_uri_t pspha_png_get_uri = {
        .uri = "/pspha.png",
        .method = HTTP_GET,
        .handler = pspha_png_get_handler,
    };
    httpd_register_uri_handler(server, &pspha_png_get_uri);

    /* URI handler for fetching favicon.ico */
    httpd_uri_t favicon_get_uri = {
        .uri = "/favicon.ico",
        .method = HTTP_GET,
        .handler = favicon_get_handler,
    };
    httpd_register_uri_handler(server, &favicon_get_uri);

    /* URI handler for fetching jquery.js */
    httpd_uri_t jquery_get_uri = {
        .uri = "/jquery.js",
        .method = HTTP_GET,
        .handler = jquery_get_handler,
    };
    httpd_register_uri_handler(server, &jquery_get_uri);

    return ESP_OK;
}

esp_err_t stop_http_server()
{
    ESP_LOGI(HTTP_TAG, "Stopping HTTP Server");
    if(httpd_stop(&server) != ESP_OK) {
        ESP_LOGE(HTTP_TAG, "stop server failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}
