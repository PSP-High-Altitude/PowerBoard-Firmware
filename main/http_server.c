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

static const char *REST_TAG = "rest-server";
#define REST_CHECK(a, str, goto_tag, ...)                                              \
    do                                                                                 \
    {                                                                                  \
        if (!(a))                                                                      \
        {                                                                              \
            ESP_LOGE(REST_TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            goto goto_tag;                                                             \
        }                                                                              \
    } while (0)

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
#define SCRATCH_BUFSIZE (10240)

typedef struct rest_server_context {
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
} rest_server_context_t;

#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

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
    char *buf = ((rest_server_context_t *)(req->user_ctx))->scratch;
    int received = 0;
    if (total_len >= SCRATCH_BUFSIZE) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return ESP_FAIL;
    }
    while (cur_len < total_len) {
        received = httpd_req_recv(req, buf + cur_len, total_len);
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
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "armed", armed);

    const char *resp = cJSON_Print(root);
    httpd_resp_sendstr(req, resp);
    free((void *)resp);
    cJSON_Delete(root);

    return ESP_OK;
}

esp_err_t start_rest_server(const char *base_path)
{
    REST_CHECK(base_path, "wrong base path", err);
    rest_server_context_t *rest_context = calloc(1, sizeof(rest_server_context_t));
    REST_CHECK(rest_context, "No memory for rest context", err);
    strlcpy(rest_context->base_path, base_path, sizeof(rest_context->base_path));

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(REST_TAG, "Starting HTTP Server");
    REST_CHECK(httpd_start(&server, &config) == ESP_OK, "Start server failed", err_start);

    /* URI handler for fetching battery status */
    httpd_uri_t battery_data_get_uri = {
        .uri = "/battery",
        .method = HTTP_GET,
        .handler = battery_data_get_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &battery_data_get_uri);

    /* URI handler for arming status */
    httpd_uri_t arm_get_uri = {
        .uri = "/arm",
        .method = HTTP_GET,
        .handler = arm_get_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &arm_get_uri);

    /* URI handler for arming control */
    httpd_uri_t arm_post_uri = {
        .uri = "/arm",
        .method = HTTP_POST,
        .handler = arm_post_handler,
        .user_ctx = rest_context
    };
    httpd_register_uri_handler(server, &arm_post_uri);

    return ESP_OK;
err_start:
    free(rest_context);
err:
    return ESP_FAIL;
}
