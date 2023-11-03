#include "max17330.h"
#include "driver/i2c.h"

esp_err_t max17330_write(max17330_conf_t conf, uint16_t addr, uint16_t* data, uint8_t data_len)
{
    uint8_t slave_addr = (addr >> 8) ? 0x16 : 0x6C;
    uint8_t *tx_buf = malloc(sizeof(uint8_t) * data_len * 2);
    xthal_memcpy((void*)tx_buf, (void*)data, data_len * 2);
    if(i2c_master_write_to_device(conf.battery, slave_addr, tx_buf, data_len * 2, 100) != ESP_OK)
    {
        return ESP_FAIL;
    }
    free(tx_buf);

    return ESP_OK;
}

esp_err_t max17330_read(max17330_conf_t conf, uint16_t addr, uint16_t* data, uint8_t data_len)
{
    uint8_t slave_addr = (addr >> 8) ? 0x16 : 0x6C;
    uint8_t *rx_buf = malloc(sizeof(uint8_t) * data_len * 2);
    if(i2c_master_read_from_device(conf.battery, slave_addr, rx_buf, data_len * 2, 100) != ESP_OK)
    {
        return ESP_FAIL;
    }
    xthal_memcpy((void*)data, (void*)rx_buf, data_len * 2);
    free(rx_buf);

    return ESP_OK;
}

esp_err_t max17330_first_time_setup(max17330_conf_t conf)
{
    // NVS should only be written a maximum of 7 times!

    // Set charging current
    uint16_t buf = 0x184B;
    if(max17330_write(conf, MAX17330_nICHGCFG, &buf, 1) != ESP_OK)
    {
        return ESP_FAIL;
    }

    // Set thermistor disabled
    buf = 0x0001;
    if(max17330_write(conf, MAX17330_nPACKCFG, &buf, 1) != ESP_OK)
    {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t max17330_init(max17330_conf_t conf)
{
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = conf.sda,
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_io_num = conf.scl,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .master.clk_speed = conf.clk,
        .clk_flags = 0,
    };

    i2c_param_config(conf.battery, &conf);

    if(i2c_driver_install(conf.battery, I2C_MODE_MASTER, 0, 0, 0) != ESP_OK)
    {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t max17330_get_battery_state(max17330_conf_t conf, battery_stat_t *stat)
{
    // Full battery capacity
    uint16_t buf = 0;
    if(max17330_read(conf, MAX17330_FULLCAPREP, &buf, 1) != ESP_OK)
    {
        return ESP_FAIL;
    }
    stat->max_cap = 0.5 * buf;

    // Current capacity
    if(max17330_read(conf, MAX17330_REPCAP, &buf, 1) != ESP_OK)
    {
        return ESP_FAIL;
    }
    stat->max_cap = 0.5 * buf;

    // Current state of charge
    if(max17330_read(conf, MAX17330_REPSOC, &buf, 1) != ESP_OK)
    {
        return ESP_FAIL;
    }
    stat->soc = buf / 25600.0;

    // Number of battery cycles
    if(max17330_read(conf, MAX17330_CYCLES, &buf, 1) != ESP_OK)
    {
        return ESP_FAIL;
    }
    stat->charge_cycles = buf / 4;

    // Time to empty (min)
    if(max17330_read(conf, MAX17330_TTE, &buf, 1) != ESP_OK)
    {
        return ESP_FAIL;
    }
    stat->tte_min = buf * 0.09375;
    
    // Time to full (min)
    if(max17330_read(conf, MAX17330_TTF, &buf, 1) != ESP_OK)
    {
        return ESP_FAIL;
    }
    stat->ttf_min = buf * 0.09375;

    // Battery age (% of max capacity)
    if(max17330_read(conf, MAX17330_AGE, &buf, 1) != ESP_OK)
    {
        return ESP_FAIL;
    }
    stat->max_cap = buf / 25600.0;

    // Charging?
    if(max17330_read(conf, MAX17330_CHGSTAT, &buf, 1) != ESP_OK)
    {
        return ESP_FAIL;
    }
    stat->charging = (buf & 0x3) ? 1 : 0;

    return ESP_OK;
}