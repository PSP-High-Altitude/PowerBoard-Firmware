#ifndef MAX17330_H
#define MAX17330_H

#include "esp_err.h"

#define MAX17330_ADDR_RAM 0x6C
#define MAX17330_ADDR_NVS 0x16

#define MAX17330_CHGSTAT 0x0A3
#define MAX17330_nCHGCFG0 0x1C2
#define MAX17330_nCHGCFG1 0x1CB
#define MAX17330_nMISCCFG2 0x1E4
#define MAX17330_nICHGTERM 0x19C
#define MAX17330_nVCHGCFG 0x1D9
#define MAX17330_nICHGCFG 0x1D8
#define MAX17330_nSTEPCHG 0x1DB
#define MAX17330_nUVPRTTH 0x1D0
#define MAX17330_nOVPRTTH 0x1DA
#define MAX17330_nODSCTH 0x1DD
#define MAX17330_nODSCCFG 0x1DE
#define MAX17330_nIPRTTH1 0x1D3
#define MAX17330_nDELAYCFG 0x1DC
#define MAX17330_nPROTCFG 0x1D7
#define MAX17330_nBATTSTATUS 0x1A8
#define MAX17330_PROTSTATUS 0x0D9
#define MAX17330_PROTALRT 0x0AF
#define MAX17330_BATT 0x0D7
#define MAX17330_CURRENT 0x01C
#define MAX17330_AVGCURRENT 0x01D
#define MAX17330_STATUS 0x000
#define MAX17330_nPACKCFG 0x1B5
#define MAX17330_nRSENSE 0x1CF
#define MAX17330_REPCAP 0x005
#define MAX17330_REPSOC 0x006
#define MAX17330_FULLCAPREP 0x010
#define MAX17330_TTE 0x011
#define MAX17330_TTF 0x020
#define MAX17330_AGE 0x007
#define MAX17330_CYCLES 0x017
#define MAX17330_DEVNAME 0x021
#define MAX17330_CHARGINGVOLTAGE 0x02A
#define MAX17330_CHARGINGCURRENT 0x028
#define MAX17330_TEMP 0x01B
#define MAX17330_COMMAND 0x060
#define MAX17330_HISTORY_WRITES 0x1FD
#define MAX17330_COMMSTAT 0x061
#define MAX17330_RESET 0x0AB

typedef enum {
    FLIGHT_BATTERY = 0,
    PYRO_BATTERY = 1,
} battery_t;

typedef struct {
    double max_cap;
    double curr_cap;
    double soc;
    uint16_t charging;
    uint16_t charge_cycles;
    double tte_min;
    double ttf_min;
    double battery_age;
    double current_mah;
    double charge_voltage;
    double charge_current;
} battery_stat_t;

typedef struct {
    battery_t battery;
    int sda;
    int scl;
    int clk;
} max17330_conf_t;

esp_err_t max17330_init(max17330_conf_t conf);

esp_err_t max17330_get_battery_state(max17330_conf_t conf, battery_stat_t *stat);

esp_err_t max17330_first_time_setup(max17330_conf_t conf);

#endif