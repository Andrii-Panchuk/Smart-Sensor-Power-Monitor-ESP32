#include "tasks.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_lcd_panel_sh1106.h"  
#include "config.h"
#include "types.h"
#include "hardware.h"
#include "event_log.h"

static const char *TAG = "tasks";

/* Shared Data */
static telemetry_data_t  s_telemetry;          
static SemaphoreHandle_t s_telemetry_mutex;
static QueueHandle_t     s_sensor_queue;       
static QueueHandle_t     s_event_queue; 

static bool s_streaming = false;
static TickType_t s_stream_last = 0;

static void task_sensors(void *pvParametrs){
    telemetry_data_t local_data = {};
    float      temp[QUANTITY_DS18B20S] = {};
    uint32_t   tick = 0;
    bool       conversion_started = false;
    TickType_t last = xTaskGetTickCount();

    while(true){
        float v = 0, i = 0, p = 0;
        if (hardware_read_power(&v, &i, &p) == ESP_OK) {
            local_data.voltage_mV = (uint32_t)(v * 1000.0f);
            local_data.current_uA = (int32_t)(i * 1000.0f);
            local_data.power_uW   = (int32_t)(p * 1000.0f);
        }

        if (tick % TEMP_PERIOD_TICKS == 0) {
            if (conversion_started) {
                for (int n = 0; n < hardware_temp_count(); n++) {
                    float t;
                    if (hardware_temp_read(n, &t)) temp[n] = t;
                }
            }
            hardware_temp_trigger();
            conversion_started = true;
        }
        tick++;

        local_data.temp_right_x100 = (int32_t)(temp[0] * 100.0f);
        local_data.temp_left_x100  = (int32_t)(temp[1] * 100.0f);
        local_data.timestamp_ms    = (uint32_t)(esp_timer_get_time() / 1000);

        if (xSemaphoreTake(s_telemetry_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            s_telemetry = local_data;
            xSemaphoreGive(s_telemetry_mutex);
        }

        xQueueSend(s_sensor_queue, &local_data, 0);

        vTaskDelayUntil(&last, pdMS_TO_TICKS(SENSORS_PERIOD_MS));
    }
}

static void task_display(void *arg)
{
    telemetry_data_t local_data;
    TickType_t last_wake_time = xTaskGetTickCount();

    while (true)
    {
        if (xSemaphoreTake(s_telemetry_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            local_data = s_telemetry;
            xSemaphoreGive(s_telemetry_mutex);

            hardware_display_draw(
                local_data.voltage_mV, 
                local_data.current_uA, 
                local_data.power_uW, 
                local_data.temp_left_x100,  
                local_data.temp_right_x100  
            );
        }

        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(DISPLAY_PERIOD_MS));
    } 
}
static void update_status_leds(bool is_fault)
{
    if (is_fault) {
        gpio_set_level(GREEN_LED, 0); 
        gpio_set_level(RED_LED, 1);   
    } else {
        gpio_set_level(GREEN_LED, 1);
        gpio_set_level(RED_LED, 0);
    }
}

static void check(bool *active, bool violated,
    event_type_t type, int32_t value, uint32_t ts)
{
    if (violated) {
        update_status_leds(true);
        if (!*active) {
            *active = true;
            event_record_t ev = { .timestamp_ms = ts, .type = type, .value = value };
            xQueueSend(s_event_queue, &ev, 0);
        }
    } else {
        update_status_leds(false);
        *active = false;
    }
}

static void task_thresholds(void *arg)
{
    bool over_current = false, over_voltage = false;
    bool under_voltage = false, over_temp = false;
    telemetry_data_t local_data;

    while (true) {
        if (xQueueReceive(s_sensor_queue, &local_data, portMAX_DELAY) != pdTRUE) continue;

        int32_t max_temp = (local_data.temp_right_x100 > local_data.temp_left_x100)
                           ? local_data.temp_right_x100 : local_data.temp_left_x100;

        check(&over_current,  local_data.current_uA > THRESHOLD_CURRENT_UA,
              EVENT_OVER_CURRENT,  local_data.current_uA / 1000,    local_data.timestamp_ms);
        check(&over_voltage,  local_data.voltage_mV > THRESHOLD_OVER_VOLTAGE_MV,
              EVENT_OVER_VOLTAGE,  (int32_t)local_data.voltage_mV,  local_data.timestamp_ms);
        check(&under_voltage, local_data.voltage_mV < THRESHOLD_UNDER_VOLTAGE_MV,
              EVENT_UNDER_VOLTAGE, (int32_t)local_data.voltage_mV,  local_data.timestamp_ms);
        check(&over_temp,     max_temp > THRESHOLD_TEMP_C_X100,
              EVENT_OVER_TEMP,     max_temp,               local_data.timestamp_ms);
    }
}

static void task_logger(void *arg)
{
    /* First entry after each startup: reason for reboot */
    event_record_t boot = {
        .timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000),
        .type         = EVENT_SYS_RESET,
        .value        = (int32_t)esp_reset_reason(),
    };
    event_log_add(&boot);

    event_record_t ev;
    while (true) {
        if (xQueueReceive(s_event_queue, &ev, portMAX_DELAY) == pdTRUE) {
            event_log_add(&ev);
        }
    }
}

static void stream_print_sample(void)
{
    telemetry_data_t local_data;

    if (xSemaphoreTake(s_telemetry_mutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
    local_data = s_telemetry;
    xSemaphoreGive(s_telemetry_mutex);

    printf("%-10" PRIu32 " | %10.3f | %12.2f | %10.2f | %9.2f | %9.2f\n",
        local_data.timestamp_ms,
        local_data.voltage_mV / 1000.0f,
        local_data.current_uA / 1000.0f,
        local_data.power_uW   / 1000.0f,
        local_data.temp_right_x100 / 100.0f,
        local_data.temp_left_x100  / 100.0f);
}

static void handle_command(const char *cmd)
{
    if (strcmp(cmd, "history") == 0) {
        event_log_print_history();
    } else if (strcmp(cmd, "stream on") == 0) {
        printf("%-10s | %10s | %12s | %10s | %9s | %9s\n",
            "Time (ms)", "Volt (V)", "Current (mA)", "Power (mW)", "Temp1 (C)", "Temp2 (C)");
        printf("-----------------------------------------------------------------------\n");
        s_streaming   = true;
        s_stream_last = xTaskGetTickCount() - pdMS_TO_TICKS(STREAM_PERIOD_MS);
    } else if (strcmp(cmd, "stream off") == 0) {
        s_streaming = false;
    }
}

static void task_uart(void *arg)
{
    char line[CMD_MAX_LEN];
    int32_t len = 0;
    uint8_t ch;

    while(true){
        if(hardware_uart_read(&ch, 1, 100) > 0){
            if(ch == '\r' || ch == '\n'){
                putchar('\n');
                if(len > 0){
                    line[len] = '\0';
                    handle_command(line);
                    len = 0;
                }
            }else if ((ch == 0x7F || ch == '\b') && len > 0) {
                len--;
                printf("\b \b");
            }else if (ch >= 0x20 && ch < 0x7F && len < CMD_MAX_LEN - 1) {
                line[len++] = (char)ch;
                putchar(ch);
            }
        }

        if (s_streaming && len == 0 &&
            (xTaskGetTickCount() - s_stream_last) >= pdMS_TO_TICKS(STREAM_PERIOD_MS)) {
            s_stream_last = xTaskGetTickCount();
            stream_print_sample();
        }

        fflush(stdout);
    }
}

void tasks_start(void)
{
    s_telemetry_mutex = xSemaphoreCreateMutex();
    s_sensor_queue    = xQueueCreate(10, sizeof(telemetry_data_t));
    s_event_queue     = xQueueCreate(20, sizeof(event_record_t));
    configASSERT(s_telemetry_mutex != NULL);
    configASSERT(s_sensor_queue != NULL);
    configASSERT(s_event_queue != NULL);

    xTaskCreate(task_sensors,    "sensors",    4096, NULL, 5, NULL);
    xTaskCreate(task_thresholds, "thresholds", 3072, NULL, 4, NULL);
    xTaskCreate(task_logger,     "logger",     4096, NULL, 4, NULL);
    xTaskCreate(task_uart,       "uart",       4096, NULL, 3, NULL);
    xTaskCreate(task_display,    "display",    4096, NULL, 2, NULL);
}