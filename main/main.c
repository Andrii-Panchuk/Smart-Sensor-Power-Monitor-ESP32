#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "hardware.h"
#include "tasks.h"
#include "event_log.h"


void app_main(void)
{
    ESP_ERROR_CHECK(hardware_init());
    event_log_init();
    tasks_start();    
}