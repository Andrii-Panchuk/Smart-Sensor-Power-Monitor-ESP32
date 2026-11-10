#ifndef HARDWARE_H
#define HARDWARE_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/* Initializes I2C, INA219, DS18B20, and the display */
esp_err_t hardware_init(void);

/* INA219 */
esp_err_t hardware_read_power(float *voltage_V, float *current_mA, float *power_mW);

/* DS18B20 */
int8_t  hardware_temp_count(void);
void hardware_temp_trigger(void);
bool hardware_temp_read(int idx, float *temp_C);
int hardware_uart_read(uint8_t *buf, size_t len, uint32_t timeout_ms);

/* Display */
esp_err_t hardware_display_draw(uint32_t voltage, uint32_t current, uint32_t power, uint32_t temp1, uint32_t temp2);

#endif /* HARDWARE_H */