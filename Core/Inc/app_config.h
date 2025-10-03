#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/**
 * @file app_config.h
 * @brief Central configuration constants for firmware timing, power, and LoRaWAN settings.
 */

#include <stdint.h>

/* LoRaWAN transmission cadence */
#define APP_SLEEP_TIME_MINUTES        10U   /**< Minutes between uplinks. */
#define APP_SLEEP_INTERVAL_SECONDS    30U   /**< RTC wake-up interval in seconds. */

/* Peripheral power budgeting */
#define APP_SENSOR_POWERUP_DELAY_MS   1000U /**< Delay for sensor power stabilization. */
#define APP_VBAT_SETTLE_DELAY_MS       300U /**< Delay after enabling VBAT measurement. */

/* LoRaWAN FPort assignments */
#define APP_FPORT_PRIMARY               1U
#define APP_FPORT_SENSOR_ERROR         10U
#define APP_FPORT_SENSOR_DISAGREE      11U

#endif /* APP_CONFIG_H */
