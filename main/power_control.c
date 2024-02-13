#include "power_control.h"
#include "driver/gpio.h"
#include <nvs.h>

#define ARM_PIN GPIO_NUM_5
uint8_t armed;
extern nvs_handle_t nvs;

const max17330_conf_t flight = {
    .battery = FLIGHT_BATTERY,
    .clk = 100000,
    .scl = GPIO_NUM_2,
    .sda = GPIO_NUM_1,
};

const max17330_conf_t pyro = {
    .battery = PYRO_BATTERY,
    .clk = 100000,
    .scl = GPIO_NUM_4,
    .sda = GPIO_NUM_3,
};

esp_err_t init_power_control()
{
    if(max17330_init(flight) != ESP_OK)
    {
        return ESP_FAIL;
    }
    if(max17330_init(pyro) != ESP_OK)
    {
        return ESP_FAIL;
    }
    /*
    if(max17330_first_time_setup(flight) != ESP_OK)
    {
        return ESP_FAIL;
    }
    if(max17330_first_time_setup(pyro) != ESP_OK)
    {
        return ESP_FAIL;
    }
    */
    return ESP_OK;
}

void set_armed()
{
    armed = 1;
    gpio_set_direction(ARM_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(ARM_PIN, 1);
    nvs_set_u8(nvs, "armed", 1);
    nvs_commit(nvs);
}

void set_disarmed()
{
    armed = 0;
    gpio_set_direction(ARM_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(ARM_PIN, 0);
    nvs_set_u8(nvs, "armed", 0);
    nvs_commit(nvs);
}

battery_stat_t get_battery(battery_t battery)
{
    battery_stat_t res = {0};
    max17330_get_battery_state(battery ? pyro : flight, &res);

    return res;
}