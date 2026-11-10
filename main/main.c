#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "hardware.h"
#include "tasks.h"
#include "event_log.h"

/*esp_err_t init_modules(void){
    i2c_master_bus_config_t i2c_bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = GPIO_NUM_21,
        .scl_io_num = GPIO_NUM_22,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = { .enable_internal_pullup = true },
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &s_i2c_bus_handle));

    ESP_ERROR_CHECK(ina219_create(s_i2c_bus_handle, INA219_DEFAULT_INFO(), &s_ina219_handle));

    // Налаштування параметрів
    ina219_config_t config = INA219_DEFAULT_CONFIG();
    config.max_expected_current_A = 3.0f;
    config.shunt_resistor_ohms = 0.1f;
    config.bus_voltage_range = INA219_BUS_VOLTAGE_RANGE_32V;

    ESP_ERROR_CHECK(ina219_config(&s_ina219_handle, config));

    ESP_ERROR_CHECK(ina219_wake_up(s_ina219_handle, INA219_MODE_SHUNT_BUS_CONT));

    onewire_bus_handle_t bus = NULL;
    onewire_bus_config_t bus_config = {
        .bus_gpio_num = GPIO_NUM_4,
        .flags = {
            .en_pull_up = true,
        }
    };

    onewire_bus_rmt_config_t rmt_config = {
        .max_rx_bytes = 10,
    };
    ESP_ERROR_CHECK(onewire_new_bus_rmt(&bus_config, &rmt_config, &bus));

    int ds18b20_device_num = 0;
    onewire_device_iter_handle_t iter = NULL;
    onewire_device_t next_onewire_device;
    esp_err_t search_result = ESP_OK;

    ESP_ERROR_CHECK(onewire_new_device_iter(bus, &iter));
    do {
        search_result = onewire_device_iter_get_next(iter, &next_onewire_device);
        if (search_result == ESP_OK) {
            ds18b20_config_t ds_cfg = {};
            onewire_device_address_t address;
            if (ds18b20_new_device_from_enumeration(&next_onewire_device, &ds_cfg, &s_ds18b20s[ds18b20_device_num]) == ESP_OK) {
                ds18b20_get_device_address(s_ds18b20s[ds18b20_device_num], &address);
                ds18b20_device_num++;
                if (ds18b20_device_num >= QUANTITY_DS18B20S) {
                    break;
                }
            }
        }
    } while (search_result != ESP_ERR_NOT_FOUND);

    return ESP_OK;
}*/

void app_main(void)
{
    ESP_ERROR_CHECK(hardware_init());
    event_log_init();
    tasks_start();    
}