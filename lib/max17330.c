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
    uint16_t buf;
    uint16_t read_buf[3];

    buf = 0xE001;   // NV Recall
    if(max17330_write(conf, MAX17330_COMMAND, &buf, 1) != ESP_OK)
    {
        return ESP_FAIL;
    }

    if(max17330_read(conf, MAX17330_nICHGCFG, read_buf, 1) != ESP_OK)
    {
        return ESP_FAIL;
    }
    if(max17330_read(conf, MAX17330_nPACKCFG, read_buf+1, 1) != ESP_OK)
    {
        return ESP_FAIL;
    }
    if(max17330_read(conf, MAX17330_nODSCTH, read_buf+2, 1) != ESP_OK)
    {
        return ESP_FAIL;
    }
    if(read_buf[0] != 0x314B || read_buf[1] != 0x0 || read_buf[2] != 0x0D04)
    {
        ESP_LOGI("MAX17330", "First time setup");
    }
    else
    {
        ESP_LOGI("MAX17330", "Already setup");
        return ESP_OK;
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

    // Set charging current
    buf = 0x314B;  // 500 mA
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

    // Set protection current threshold
    buf = 0xD04;
    if(max17330_write(conf, MAX17330_nODSCTH, &buf, 1) != ESP_OK)
    {
        return ESP_FAIL;
    }

    buf = 0xE904;   // Copy NV block
    if(max17330_write(conf, MAX17330_COMMAND, &buf, 1) != ESP_OK)
    {
        return ESP_FAIL;
    }
    vTaskDelay(8 / portTICK_PERIOD_MS); // Wait tBLOCK

    // POR
    buf = 0x8000;
    if(max17330_write(conf, MAX17330_RESET, &buf, 1) != ESP_OK)
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
    }
    else
    {
        return ESP_FAIL;
    }

    if(max17330_read(conf, MAX17330_PROTALRT, &buf, 1) != ESP_OK)
    {
        return ESP_FAIL;
    }
    printf("Protection alert: 0x%x\n", buf);

    return ESP_OK;
}

esp_err_t max17330_reset(max17330_conf_t conf)
{
    // Hardware Reset
    uint16_t buf = 0x000F;
    if(max17330_write(conf, 0x060, &buf, 1) != ESP_OK)
    {
        return ESP_FAIL;
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);

    // Configuration Reset
    buf = 0x8000;
    if(max17330_write(conf, MAX17330_RESET, &buf, 1) != ESP_OK)
    {
        return ESP_FAIL;
    }
    printf("Resetting MAX17330");
    esp_err_t err;
    while((err = max17330_read(conf, MAX17330_RESET, &buf, 1)) == ESP_OK && (buf & 0x8000) != 0)
    {
        printf(".");
        vTaskDelay(1 / portTICK_PERIOD_MS);
    }
    printf("\n");
    if(err != ESP_OK)
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

    /*
    // Charging?
    if(max17330_read(conf, MAX17330_PCKP, &buf, 1) != ESP_OK)
    {
        return ESP_FAIL;
    }
    stat->charging = (0.3125e-3 * buf) > 4.5;
    if(max17330_read(conf, MAX17330_CHGSTAT, &buf, 1) != ESP_OK)
    {
        return ESP_FAIL;
    }
    stat->charging = stat->charging && (buf & 0x3);
    */

    // Current
    if(max17330_read(conf, MAX17330_AVGCURRENT, &buf, 1) != ESP_OK)
    {
        return ESP_FAIL;
    }
    stat->current_mah = 0.15625 * ((int16_t)buf);
    stat->charging = stat->current_mah > 10;

    // Voltage
    if(max17330_read(conf, MAX17330_VCELL, &buf, 1) != ESP_OK)
    {
        return ESP_FAIL;
    }
    stat->batt_voltage = 78.125e-6 * buf;

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
    
    return ESP_OK;
}