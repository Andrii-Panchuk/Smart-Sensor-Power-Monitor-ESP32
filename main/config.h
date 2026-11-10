#ifndef CONFIG_H
#define CONFIG_H

#include "driver/gpio.h"

// Pins
#define PIN_I2C_SDA                 GPIO_NUM_21
#define PIN_I2C_SCL                 GPIO_NUM_22
#define PIN_ONEWIRE                 GPIO_NUM_4
#define GREEN_LED                   GPIO_NUM_2
#define RED_LED                     GPIO_NUM_15

// Uart
#define UART_PORT                   UART_NUM_0
#define UART_BAUD_RATE              115200
#define UART_RX_BUF_SIZE            256
#define CMD_MAX_LEN                 32
#define STREAM_PERIOD_MS            1000

// I2C addresses
#define DISPLAY_I2C_ADDR            0x3C
#define INA219_I2C_ADDR             0x40

// INA219 parameters
#define INA219_SHUNT_OHMS           0.1f
#define INA219_MAX_CURRENT_A        3.0f

// DS18B20S parameters
#define QUANTITY_DS18B20S           2

// Periods
#define SENSORS_PERIOD_MS           500           
#define TEMP_PERIOD_TICKS           2            
#define DISPLAY_PERIOD_MS           34    

// Protection thresholds
#define THRESHOLD_CURRENT_UA        3000000       
#define THRESHOLD_OVER_VOLTAGE_MV   24000         
#define THRESHOLD_UNDER_VOLTAGE_MV  10000         
#define THRESHOLD_TEMP_C_X100       4000    

// Event log
#define EVENT_LOG_MAX               200

#endif // CONFIG_H