#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Initialize watchdog with desired timeout.
 * lsi_hz: Use LSI_MAX (56000) for worst-case safety, or 0 to default to max.
 */
bool watchdog_init(uint32_t desired_timeout_ms, uint32_t lsi_hz,
                   uint32_t *actual_timeout_ms);

/* Mark watchdog as started (e.g., when initialized by HAL). */
void watchdog_mark_started(void);

/*
 * Initialize watchdog for >= 90 second timeout at max LSI.
 * Uses maximum reload value for longest possible timeout.
 * Returns actual timeout in ms via pointer if provided.
 */
bool watchdog_init_90s(uint32_t *actual_timeout_ms);

bool watchdog_update(uint32_t desired_timeout_ms, uint32_t lsi_hz,
                     uint32_t *actual_timeout_ms);
void watchdog_kick(void);
bool watchdog_is_started(void);

#endif /* WATCHDOG_H */
