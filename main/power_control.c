#include "power_control.h"
#include "driver/gpio.h"

#define ARM_PIN 16
uint8_t armed;

const max17330_conf_t flight = {
    .battery = FLIGHT_BATTERY,
    .charge_current = 250,
    .clk = 400000,
    .scl = 9,
    .sda = 10,
};

const max17330_conf_t pyro = {
    .battery = FLIGHT_BATTERY,
    .charge_current = 250,
    .clk = 400000,
    .scl = 9,
    .sda = 10,
};

esp_err_t init_power_control()
{
    gpio_set_direction(ARM_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(ARM_PIN, 0);
    armed = 0;

    if(max17330_init(flight) != ESP_OK)
    {
        return ESP_FAIL;
    }
    if(max17330_init(pyro) != ESP_OK)
    {
        return ESP_FAIL;
    }

    return ESP_OK;
}

void set_armed()
{
    armed = 1;
    gpio_set_level(ARM_PIN, 1);
}

void set_disarmed()
{
    armed = 0;
    gpio_set_level(ARM_PIN, 0);
}

battery_stat_t get_battery(battery_t battery)
{
    battery_stat_t res = {0};
    max17330_get_battery_state(battery ? pyro : flight, &res);

    return res;
}