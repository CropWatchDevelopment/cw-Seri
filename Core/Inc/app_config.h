#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/**
 * @file app_config.h
 * @brief Central configuration constants for firmware timing, power, and LoRaWAN settings.
 */

#include <stdint.h>

/* LoRaWAN transmission cadence */
#define APP_SLEEP_TIME_MINUTES        1U   /**< Minutes between uplinks. */
#define APP_SLEEP_INTERVAL_SECONDS    30U   /**< RTC wake-up interval in seconds. */

/* LSE diagnostic build configuration */
#define APP_LSE_TEST_MODE                 1U /**< Set to 1 to enable dedicated LSE crystal diagnostics. */
#define APP_LSE_TEST_WAKE_SECONDS         10U /**< Target RTC wake interval (seconds) during LSE test. */
#define APP_LSE_TEST_EXPECTED_INTERVAL_MS (APP_LSE_TEST_WAKE_SECONDS * 1000UL)
#define APP_LSE_TEST_DRIFT_TOLERANCE_MS   250UL /**< Acceptable drift before logging a warning. */

/* Peripheral power budgeting */
#define APP_SENSOR_POWERUP_DELAY_MS   1000U /**< Delay for sensor power stabilization. */
#define APP_VBAT_SETTLE_DELAY_MS       300U /**< Delay after enabling VBAT measurement. */

/* LoRaWAN FPort assignments */
#define APP_FPORT_PRIMARY               1U
#define APP_FPORT_SENSOR_ERROR         10U
#define APP_FPORT_SENSOR_DISAGREE      11U

#endif /* APP_CONFIG_H */
