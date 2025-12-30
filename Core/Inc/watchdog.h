#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <stdbool.h>
#include <stdint.h>

bool watchdog_init(uint32_t desired_timeout_ms, uint32_t lsi_hz,
                   uint32_t *actual_timeout_ms);
bool watchdog_update(uint32_t desired_timeout_ms, uint32_t lsi_hz,
                     uint32_t *actual_timeout_ms);
void watchdog_kick(void);
bool watchdog_is_started(void);

#endif /* WATCHDOG_H */
