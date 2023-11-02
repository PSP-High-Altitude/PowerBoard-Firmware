#ifndef POWER_CONTROL_H
#define POWER_CONTROL_H

#include "stdint.h"

typedef struct {
    double max_cap;
    double curr_cap;
    double soc;
    uint8_t charging;
} battery_stat_t;

void init_power_ctrl();
void set_armed();
void set_disarmed();
battery_stat_t get_battery(uint8_t battery);

#endif