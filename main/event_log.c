#include "event_log.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "config.h"

static const char *TAG = "event_log";

#define NVS_NAMESPACE   "monitor"
#define NVS_KEY         "events"

typedef struct {
    uint32_t       count;              
    uint32_t       head;                
    event_record_t items[EVENT_LOG_MAX];
} event_log_t;

static event_log_t       s_log;
static nvs_handle_t      s_nvs;
static SemaphoreHandle_t s_mutex;

void event_log_init(void){
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    
    ESP_ERROR_CHECK(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &s_nvs));

    size_t size = sizeof(s_log);
    err = nvs_get_blob(s_nvs, NVS_KEY, &s_log, &size);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "The log is empty (first launch)");
        memset(&s_log, 0, sizeof(s_log));
    } else if (err != ESP_OK || size != sizeof(s_log) ||
               s_log.count > EVENT_LOG_MAX || s_log.head >= EVENT_LOG_MAX) {
        ESP_LOGW(TAG, "The logbook is invalid; cleared");
        memset(&s_log, 0, sizeof(s_log));
    } else {
        ESP_LOGI(TAG, "Events loaded: %" PRIu32, s_log.count);
    }

    s_mutex = xSemaphoreCreateMutex();
    configASSERT(s_mutex != NULL);
}

void event_log_add(const event_record_t *ev)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    s_log.items[s_log.head] = *ev;                      
    s_log.head = (s_log.head + 1) % EVENT_LOG_MAX;      
    if (s_log.count < EVENT_LOG_MAX) s_log.count++;
    else s_log.count = 0;

    esp_err_t err = nvs_set_blob(s_nvs, NVS_KEY, &s_log, sizeof(s_log));
    if (err == ESP_OK) err = nvs_commit(s_nvs);         

    xSemaphoreGive(s_mutex);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save: %s", esp_err_to_name(err));
    }
}

static const char *reset_name(int32_t r)
{
    switch (r) {
        case ESP_RST_POWERON:  return "POWER_ON";
        case ESP_RST_SW:       return "SOFTWARE";
        case ESP_RST_PANIC:    return "PANIC";
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT:      return "WATCHDOG";
        case ESP_RST_BROWNOUT: return "BROWNOUT";
        default:               return "OTHER";
    }
}

void event_log_print_history(void)
{
    static event_log_t copy;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    copy = s_log;
    xSemaphoreGive(s_mutex);

    printf("%-4s | %-10s | %-15s | %-20s\n", "N", "Time (ms)", "Event", "Value");
    printf("-----------------------------------------------------------\n");

    uint32_t start = (copy.count < EVENT_LOG_MAX) ? 0 : copy.head;

    for (uint32_t i = 0; i < copy.count; i++) {
        const event_record_t *e = &copy.items[(start + i) % EVENT_LOG_MAX];

        printf("%-4" PRIu32 " | %-10" PRIu32 " | ", i + 1, e->timestamp_ms);

        switch (e->type) {
            case EVENT_SYS_RESET:
                printf("%-15s | %-20s\n", "RESET", reset_name(e->value));
                break;

            case EVENT_OVER_CURRENT: {
                char buf[21];
                snprintf(buf, sizeof(buf), "%" PRId32 " mA", e->value);
                printf("%-15s | %-20s\n", "OVER_CURRENT", buf);
                break;
            }

            case EVENT_OVER_VOLTAGE: {
                char buf[21];
                snprintf(buf, sizeof(buf), "%" PRId32 " mV", e->value);
                printf("%-15s | %-20s\n", "OVER_VOLTAGE", buf);
                break;
            }

            case EVENT_UNDER_VOLTAGE: {
                char buf[21];
                snprintf(buf, sizeof(buf), "%" PRId32 " mV", e->value);
                printf("%-15s | %-20s\n", "UNDER_VOLTAGE", buf);
                break;
            }

            case EVENT_OVER_TEMP: {
                char buf[21];
                snprintf(buf, sizeof(buf), "%.2f C", e->value / 100.0f);
                printf("%-15s | %-20s\n", "OVER_TEMP", buf);
                break;
            }

            default: {
                char buf[21];
                snprintf(buf, sizeof(buf), "%" PRId32, e->value);
                printf("%-15s | %-20s\n", "UNKNOWN", buf);
                break;
            }
        }
    }
    printf("-----------------------------------------------------------\n");
}