#ifndef POWER_CONTROL_H
#define POWER_CONTROL_H

#include "max17330.h"
#include "stdint.h"

esp_err_t init_power_control();
void set_armed();
void set_disarmed();
battery_stat_t get_battery(battery_t battery);

#endif