#include "power_control.h"
#include "driver/gpio.h"

#define ARM_PIN 16
uint8_t armed;

void init_power_control()
{
    gpio_set_direction(ARM_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(ARM_PIN, 0);
    armed = 0;
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

battery_stat_t get_battery(uint8_t battery)
{
    battery_stat_t res = {0};
    //Test
    res.charging = 0;
    res.curr_cap = 800;
    res.max_cap = 1000;
    res.soc = 0.8;

    return res;
}