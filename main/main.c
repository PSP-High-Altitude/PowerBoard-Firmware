/* HTTP Restful API Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "esp_vfs_fat.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "mdns.h"
#include "lwip/apps/netbiosns.h"
#include "power_control.h"

#define MDNS_INSTANCE "ESPMDNS"
#define MDNS_HOST_NAME "powerboard"

// Change for each board
#define APP_SSID "Powerboard1"
#define APP_CHANNEL 1
static const char *TAG = "Powerboard1";

nvs_handle_t nvs;

esp_err_t start_rest_server(const char *base_path);

static void initialise_mdns(void)
{
    mdns_init();
    mdns_hostname_set(MDNS_HOST_NAME);
    mdns_instance_name_set(MDNS_INSTANCE);

    mdns_txt_item_t serviceTxtData[] = {
        {"board", "esp32"},
        {"path", "/"}
    };

    ESP_ERROR_CHECK(mdns_service_add("ESP32-WebServer", "_http", "_tcp", 80, serviceTxtData,
                                     sizeof(serviceTxtData) / sizeof(serviceTxtData[0])));
}

esp_err_t init_fs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "www",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = false
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    return ESP_OK;
}

esp_err_t init_wifi(void)
{
    wifi_init_config_t init_conf = WIFI_INIT_CONFIG_DEFAULT();
    wifi_config_t conf = {
        .ap = {
            .ssid = APP_SSID,
            .ssid_len = strlen(APP_SSID),
            .channel = APP_CHANNEL,
            .password = "",
            .max_connection = 5,
            .authmode = WIFI_AUTH_OPEN,
        },
    };
    if(esp_wifi_init(&init_conf) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize WiFi");
        return ESP_FAIL;
    }
    if(esp_wifi_set_mode(WIFI_IF_AP) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set WiFi mode");
        return ESP_FAIL;
    }
    if(esp_wifi_set_config(WIFI_IF_AP, &conf) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set WiFi config");
        return ESP_FAIL;
    }
    if(esp_wifi_start() != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start WiFi");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "WiFi AP started");

    return ESP_OK;
}

esp_err_t init_nvs(void)
{
    uint8_t nvs_armed;
    if(nvs_open("nvs", NVS_READWRITE, &nvs) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open NVS");
        return ESP_FAIL;
    }
    if(nvs_get_u8(nvs, "armed", &nvs_armed) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read armed from NVS");
        return ESP_FAIL;
    }
    if(nvs_armed)
    {
        set_armed();
    }
    else
    {
        set_disarmed();
    }
    ESP_LOGI(TAG, "NVS initialized and armed set to %d", nvs_armed);
    return ESP_OK;
}

void app_main(void)
{
    // Prioritize loading the last state of the board
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(init_nvs());

    // Then handle the rest
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    initialise_mdns();
    netbiosns_init();
    netbiosns_set_name("powerboard");

    ESP_ERROR_CHECK(init_power_control());
    ESP_ERROR_CHECK(init_wifi());
    ESP_ERROR_CHECK(init_fs());
    ESP_ERROR_CHECK(start_rest_server("/www"));
}
