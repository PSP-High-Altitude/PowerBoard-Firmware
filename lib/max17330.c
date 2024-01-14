#include "max17330.h"
#include "driver/i2c.h"
#include "esp_log.h"

esp_err_t max17330_write(max17330_conf_t conf, uint16_t addr, uint16_t* data, uint8_t data_len)
{
    uint8_t slave_addr = (addr > 0xFF) ? 0x16 : 0x6C;
    slave_addr >>= 1; // The given addresses are 8-bit
    uint8_t *tx_buf = malloc(sizeof(uint8_t) * data_len * 2 + sizeof(uint8_t));
    tx_buf[0] = addr & 0xFF;
    xthal_memcpy((void*)(tx_buf + 1), (void*)data, data_len * 2);
    if(i2c_master_write_to_device(conf.battery, slave_addr, tx_buf, data_len * 2 + 1, 100) != ESP_OK)
    {
        return ESP_FAIL;
    }
    free(tx_buf);

    return ESP_OK;
}

esp_err_t max17330_read(max17330_conf_t conf, uint16_t addr, uint16_t* data, uint8_t data_len)
{
    uint8_t slave_addr = (addr > 0xFF) ? 0x16 : 0x6C;
    slave_addr >>= 1; // The given addresses are 8-bit
    uint8_t *rx_buf = malloc(sizeof(uint8_t) * data_len * 2);
    uint8_t tx_buf = addr & 0xFF;
    if(i2c_master_write_read_device(conf.battery, slave_addr, &tx_buf, 1, rx_buf, data_len * 2, 100) != ESP_OK)
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

    // Set precharge current
    uint16_t buf = 0x2001;
    if(max17330_write(conf, MAX17330_nCHGCFG0, &buf, 1) != ESP_OK)
    {
        return ESP_FAIL;
    }

    // Set charging current
    buf = 0x184B;  // 250 mA
    if(max17330_write(conf, MAX17330_nICHGCFG, &buf, 1) != ESP_OK)
    {
        return ESP_FAIL;
    }

    // Set thermistor disabled
    buf = 0x0;
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
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = conf.scl,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = conf.clk,
    };

    i2c_reset_tx_fifo(conf.battery == FLIGHT_BATTERY ? I2C_NUM_0 : I2C_NUM_1);
    i2c_reset_rx_fifo(conf.battery == FLIGHT_BATTERY ? I2C_NUM_0 : I2C_NUM_1);

    if(i2c_param_config(conf.battery == FLIGHT_BATTERY ? I2C_NUM_0 : I2C_NUM_1, &i2c_conf) != ESP_OK)
    {
        return ESP_FAIL;
    }

    int err;
    if((err = i2c_driver_install(conf.battery == FLIGHT_BATTERY ? I2C_NUM_0 : I2C_NUM_1, I2C_MODE_MASTER, 0, 0, 0)) != ESP_OK)
    {
        return ESP_FAIL;
    }

    uint16_t buf = 0;
    if(max17330_read(conf, MAX17330_DEVNAME, &buf, 1) != ESP_OK)
    {
        ESP_LOGE("MAX17330", "%d, Failed to read from MAX17330", conf.battery);
        return ESP_FAIL;
    }
    if(buf != 0x40B0 && buf != 0x40B1)
    {
        ESP_LOGI("MAX17330", "%d, Incorrect device ID", conf.battery);
        return ESP_FAIL;
    }

    // Unlock write protection
    for(uint8_t i = 0; i < 2; i++)
    {
        buf = 0x0;
        if(max17330_write(conf, MAX17330_COMMSTAT, &buf, 1) != ESP_OK)
        {
            return ESP_FAIL;
        }
    }

    // Number of writes
    buf = 0xE29B;
    if(max17330_write(conf, MAX17330_COMMAND, &buf, 1) != ESP_OK)
    {
        return ESP_FAIL;
    }
    vTaskDelay(5 / portTICK_PERIOD_MS); // Wait tRECALL
    if(max17330_read(conf, MAX17330_HISTORY_WRITES, &buf, 1) != ESP_OK)
    {
        return ESP_FAIL;
    }
    if(buf != 0x0000)
    {
        buf = (buf >> 8) | (buf & 0xFF);
        uint8_t count = 0;
        while(!(buf & 0x80))
        {
            buf <<= 1;
            count++;
        }
        ESP_LOGI("MAX17330", "%d, Number of writes remaining: %d", conf.battery, count);
        return ESP_OK;
    }
    else
    {
        return ESP_FAIL;
    }

    // Set thermistor disabled
    buf = 0x1;
    if(max17330_write(conf, MAX17330_nPACKCFG, &buf, 1) != ESP_OK)
    {
        return ESP_FAIL;
    }

    // POR
    buf = 0x8000;
    if(max17330_write(conf, MAX17330_RESET, &buf, 1) != ESP_OK)
    {
        return ESP_FAIL;
    }
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
    stat->curr_cap = 0.5 * buf;

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
    stat->battery_age = buf / 25600.0;

    // Charging?
    if(max17330_read(conf, MAX17330_CHGSTAT, &buf, 1) != ESP_OK)
    {
        return ESP_FAIL;
    }
    stat->charging = buf;

    // Current
    if(max17330_read(conf, MAX17330_AVGCURRENT, &buf, 1) != ESP_OK)
    {
        return ESP_FAIL;
    }
    stat->current_mah = 0.15625 * ((int16_t)buf);

    // Charge Voltage
    if(max17330_read(conf, MAX17330_CHARGINGVOLTAGE, &buf, 1) != ESP_OK)
    {
        return ESP_FAIL;
    }
    stat->charge_voltage = 78.125e-6 * buf;

    // Charge Current
    if(max17330_read(conf, MAX17330_CHARGINGCURRENT, &buf, 1) != ESP_OK)
    {
        return ESP_FAIL;
    }
    stat->charge_current = 0.15625 * ((int16_t)buf);

    // Charge Current
    if(max17330_read(conf, MAX17330_nPACKCFG, &buf, 1) != ESP_OK)
    {
        return ESP_FAIL;
    }
    ESP_LOGI("MAX17330", "%d, PACKCFG: %d", conf.battery, buf);
    //ESP_LOGI("MAX17330", "%d, TEMP: %f", conf.battery, (int16_t)buf * (1.0 / 256.0));

    return ESP_OK;
}