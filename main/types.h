#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

/* A single count of all sensors */
typedef struct {
    uint32_t voltage_mV;
    int32_t  current_uA;        
    int32_t  power_uW;
    int32_t  temp_right_x100;   
    int32_t  temp_left_x100;
    uint32_t timestamp_ms;
} telemetry_data_t;

/* Event types in the log */
typedef enum {
    EVENT_NONE = 0,
    EVENT_SYS_RESET,
    EVENT_OVER_CURRENT,
    EVENT_OVER_VOLTAGE,
    EVENT_UNDER_VOLTAGE,
    EVENT_OVER_TEMP,
} event_type_t;

/* One journal entry */
typedef struct {
    uint32_t timestamp_ms;
    uint32_t type;              
    int32_t  value;             
} event_record_t;

#endif /* TYPES_H */