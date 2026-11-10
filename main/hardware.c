#include "hardware.h"
#include "driver/i2c_master.h"
#include "driver/uart.h"
#include "ina219.h"
#include "onewire_bus.h"
#include "ds18b20.h"
#include "esp_lcd_io_i2c.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_sh1106.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "esp_check.h"
#include "esp_log.h"
#include "config.h"

static const char *TAG = "hardware";

static i2c_master_bus_handle_t   s_i2c_bus;
static ina219_handle_t           s_ina219;
static onewire_bus_handle_t      s_onewire;
static esp_lcd_panel_io_handle_t io_handle = NULL;
static esp_lcd_panel_handle_t    panel_handle = NULL;
static ds18b20_device_handle_t   s_ds18b20s[QUANTITY_DS18B20S];
static int8_t                    s_ds18b20_count = 0;

static lv_obj_t *s_label_volts;
static lv_obj_t *s_label_current;
static lv_obj_t *s_label_power;
static lv_obj_t *s_label_temp1;
static lv_obj_t *s_label_temp2;

static esp_err_t init_led(void){
    gpio_config_t green_led_cfg = {
        .pin_bit_mask = (1ULL << GREEN_LED),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config_t red_led_cfg = {
        .pin_bit_mask = (1ULL << RED_LED),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&green_led_cfg));
    ESP_ERROR_CHECK(gpio_config(&red_led_cfg));

    return ESP_OK;
}

static esp_err_t init_i2c(void){
    i2c_master_bus_config_t cfg = {
        .i2c_port          = I2C_NUM_0,
        .sda_io_num        = PIN_I2C_SDA,
        .scl_io_num        = PIN_I2C_SCL,
        .clk_source        = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags             = { .enable_internal_pullup = true },
    };
    return i2c_new_master_bus(&cfg, &s_i2c_bus);
}

static esp_err_t init_ina219(void){
    ESP_RETURN_ON_ERROR(i2c_master_probe(s_i2c_bus, INA219_I2C_ADDR, 100),
                        TAG, "INA219 is not associated with the address 0x%02X", INA219_I2C_ADDR);

    ESP_ERROR_CHECK(ina219_create(s_i2c_bus, INA219_DEFAULT_INFO(), &s_ina219));

    ina219_config_t cfg = INA219_DEFAULT_CONFIG();
    cfg.max_expected_current_A = INA219_MAX_CURRENT_A;
    cfg.shunt_resistor_ohms    = INA219_SHUNT_OHMS;
    cfg.bus_voltage_range      = INA219_BUS_VOLTAGE_RANGE_32V;

    ESP_ERROR_CHECK(ina219_config(&s_ina219, cfg));

    ESP_ERROR_CHECK(ina219_wake_up(s_ina219, INA219_MODE_SHUNT_BUS_CONT));
    return ESP_OK;
}

static esp_err_t init_ds18b20(void){
    onewire_bus_config_t bus_cfg = {
        .bus_gpio_num = PIN_ONEWIRE,
        .flags = { .en_pull_up = true },
    };

    onewire_bus_rmt_config_t rmt_cfg = { .max_rx_bytes = 10 };

    ESP_ERROR_CHECK(onewire_new_bus_rmt(&bus_cfg, &rmt_cfg, &s_onewire));

    onewire_device_iter_handle_t iter = NULL;
    onewire_device_t dev;

    ESP_ERROR_CHECK(onewire_new_device_iter(s_onewire, &iter));
    esp_err_t res;

    do {
        res = onewire_device_iter_get_next(iter, &dev);
        if (res == ESP_OK) {
            ds18b20_config_t ds_cfg = {};
            if (ds18b20_new_device_from_enumeration(&dev, &ds_cfg,
                                                    &s_ds18b20s[s_ds18b20_count]) == ESP_OK) {
                if (++s_ds18b20_count >= QUANTITY_DS18B20S) break;
            }
        }
    } while (res != ESP_ERR_NOT_FOUND);

    onewire_del_device_iter(iter);
    ESP_LOGI(TAG, "DS18B20 Found: %d", s_ds18b20_count);
    return ESP_OK;
}

static esp_err_t init_display(void){
    esp_lcd_panel_io_i2c_config_t io_config = ESP_SH1106_DEFAULT_IO_CONFIG;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(s_i2c_bus, &io_config, &io_handle));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = -1,                         
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,   
        .data_endian = LCD_RGB_DATA_ENDIAN_LITTLE,    
        .bits_per_pixel = SH1106_PIXELS_PER_BYTE / 8, 
        .flags = {
            .reset_active_high = false,               
        },
        .vendor_config = NULL,                        
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_sh1106(io_handle, &panel_config, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&lvgl_cfg);

    const lvgl_port_display_cfg_t disp_cfg = {
        .panel_handle = panel_handle,
        .io_handle = io_handle,
        .buffer_size = 128 * 64,
        .double_buffer = false,
        .hres = 128,
        .vres = 64,
        .monochrome = true,
        .flags = {
            .buff_dma = true,
        }
    };
    lv_disp_t *disp = lvgl_port_add_disp(&disp_cfg);

    if (lvgl_port_lock(0)) {
        lv_obj_t *scr = lv_scr_act();

        s_label_volts = lv_label_create(scr);
        lv_label_set_text(s_label_volts, "V: 0.00V");
        lv_obj_align(s_label_volts, LV_ALIGN_TOP_LEFT, 0, 0);

        s_label_current = lv_label_create(scr);
        lv_label_set_text(s_label_current, "I: 0.00A");
        lv_obj_align(s_label_current, LV_ALIGN_TOP_LEFT, 0, 22);

        s_label_power = lv_label_create(scr);
        lv_label_set_text(s_label_power, "P: 0.0W");
        lv_obj_align(s_label_power, LV_ALIGN_TOP_LEFT, 0, 44);

        s_label_temp1 = lv_label_create(scr);
        lv_label_set_text(s_label_temp1, "T1: 0.0C");
        lv_obj_align(s_label_temp1, LV_ALIGN_TOP_LEFT, 68, 0);

        s_label_temp2 = lv_label_create(scr);
        lv_label_set_text(s_label_temp2, "T2: 0.0C");
        lv_obj_align(s_label_temp2, LV_ALIGN_TOP_LEFT, 68, 22);

        lvgl_port_unlock();
    }

    return ESP_OK;
}

static esp_err_t init_uart(void)
{
    const uart_config_t cfg = {
        .baud_rate  = UART_BAUD_RATE,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, UART_RX_BUF_SIZE, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &cfg));
    return ESP_OK;
}

//  Public Interface
esp_err_t hardware_init(void){
    ESP_RETURN_ON_ERROR(init_i2c(),     TAG, "i2c");
    ESP_RETURN_ON_ERROR(init_ina219(),  TAG, "ina219");
    ESP_RETURN_ON_ERROR(init_ds18b20(), TAG, "ds18b20");
    ESP_RETURN_ON_ERROR(init_display(), TAG, "display");
    ESP_RETURN_ON_ERROR(init_uart(),    TAG, "uart");
    ESP_RETURN_ON_ERROR(init_led(),     TAG, "led");
    return ESP_OK;
}

esp_err_t hardware_read_power(float *voltage_V, float *current_mA, float *power_mW)
{
    ESP_RETURN_ON_ERROR(ina219_get_bus_voltage_V(s_ina219, voltage_V), TAG, "voltage");
    ESP_RETURN_ON_ERROR(ina219_get_current_mA(s_ina219, current_mA),   TAG, "current");
    ESP_RETURN_ON_ERROR(ina219_get_power_mW(s_ina219, power_mW),       TAG, "power");
    return ESP_OK;
}

int8_t hardware_temp_count(void) { return s_ds18b20_count; }

void hardware_temp_trigger(void)
{
    for (int i = 0; i < s_ds18b20_count; i++) {
        ds18b20_trigger_temperature_conversion(s_ds18b20s[i]);
    }
}

bool hardware_temp_read(int idx, float *temp_C)
{
    if (idx < 0 || idx >= s_ds18b20_count) return false;
    return ds18b20_get_temperature(s_ds18b20s[idx], temp_C) == ESP_OK;
}

int hardware_uart_read(uint8_t *buf, size_t len, uint32_t timeout_ms)
{
    return uart_read_bytes(UART_PORT, buf, len, pdMS_TO_TICKS(timeout_ms));
}

esp_err_t hardware_display_draw(uint32_t voltage, uint32_t current, uint32_t power, uint32_t temp1, uint32_t temp2)
{
    
    char buf[32];

    // Розраховуємо float значення
    float v_val = voltage / 1000.0f;
    float i_val = current / 1000.0f;     // якщо в uA — діліть на 1000000.0f
    float p_val = power / 1000.0f;       // якщо в uW — діліть на 1000000.0f
    float t1_val = temp1 / 100.0f;       // якщо temp_x100
    float t2_val = temp2 / 100.0f;

    if (lvgl_port_lock(0)) {
        snprintf(buf, sizeof(buf), "V: %.2fV", v_val);
        lv_label_set_text(s_label_volts, buf);

        snprintf(buf, sizeof(buf), "I: %.2fmA", i_val);
        lv_label_set_text(s_label_current, buf);

        snprintf(buf, sizeof(buf), "P: %.1fW", p_val);
        lv_label_set_text(s_label_power, buf);

        snprintf(buf, sizeof(buf), "T1:%.1fC", t1_val);
        lv_label_set_text(s_label_temp1, buf);

        snprintf(buf, sizeof(buf), "T2:%.1fC", t2_val);
        lv_label_set_text(s_label_temp2, buf);

        lvgl_port_unlock();
    }
    return ESP_OK;
}