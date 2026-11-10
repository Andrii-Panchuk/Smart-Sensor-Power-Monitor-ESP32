#ifndef EVENT_LOG_H
#define EVENT_LOG_H

#include "types.h"

void event_log_init(void);

void event_log_add(const event_record_t *ev);

void event_log_print_history(void);

#endif /* EVENT_LOG_H */