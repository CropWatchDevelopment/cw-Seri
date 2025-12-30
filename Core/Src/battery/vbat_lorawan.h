#pragma once
/*
 * vbat_lorawan.h
 *
 * Battery measurement + LoRaWAN DevStatusAns battery encoding
 * - Integer-only, STM32 HAL
 * - Assumes 1M:1M divider (×2), buffered, gated by VBAT_MEAS_EN
 * - Optional "under load" min measurement using a load-enable GPIO (default: I2C_ENABLE)
 * - ADC tips: use long sampling time, discard first sample after enabling, and average
 *
 * Configure the macros below for your board.
 */

#include <stdint.h>
#include <stdbool.h>
#include "stm32l0xx_hal.h"   /* or your STM32 family header */

/* ===== Board-specific pins (EDIT THESE) ===== */
#ifndef VBAT_MEAS_EN_GPIO_Port
#define VBAT_MEAS_EN_GPIO_Port   GPIOB
#endif

#ifndef VBAT_MEAS_EN_Pin
#define VBAT_MEAS_EN_Pin         GPIO_PIN_0
#endif

/* 1 = VBAT_MEAS_EN high enables measurement; 0 = low enables measurement */
#ifndef VBAT_MEAS_EN_ACTIVE_HIGH
#define VBAT_MEAS_EN_ACTIVE_HIGH 1
#endif

/* ===== Load-enable pin used to create a known load for the 2nd read ===== */
#ifndef VBAT_LOAD_EN_GPIO_Port
#if defined(I2C_ENABLE_GPIO_Port)
#define VBAT_LOAD_EN_GPIO_Port   I2C_ENABLE_GPIO_Port
#else
#define VBAT_LOAD_EN_GPIO_Port   GPIOB
#endif
#endif

#ifndef VBAT_LOAD_EN_Pin
#if defined(I2C_ENABLE_Pin)
#define VBAT_LOAD_EN_Pin         I2C_ENABLE_Pin
#else
#define VBAT_LOAD_EN_Pin         GPIO_PIN_5
#endif
#endif

#ifndef VBAT_LOAD_EN_ACTIVE_HIGH
#define VBAT_LOAD_EN_ACTIVE_HIGH 1
#endif

/* ===== ADC & reference ===== */
#ifndef ADC_RES_BITS
#define ADC_RES_BITS             12
#endif

#ifndef VREF_mV
#define VREF_mV                  3300U      /* If you use Vrefint, update at runtime in your app */
#endif

#ifndef ADC_MAX_COUNTS
#define ADC_MAX_COUNTS           ((1U << ADC_RES_BITS) - 1U)
#endif

/* Divider factor: 1M:1M → ×2 */
#ifndef VBAT_DIV_NUM
#define VBAT_DIV_NUM             2U
#endif
#ifndef VBAT_DIV_DEN
#define VBAT_DIV_DEN             1U
#endif

/* ===== Timing / sampling ===== */
#ifndef VBAT_SETTLE_MS
#define VBAT_SETTLE_MS           20U        /* op-amp + node settle (no C16) */
#endif

#ifndef VBAT_SAMPLES
#define VBAT_SAMPLES             16U        /* average count */
#endif

/* ===== Loaded measurement ===== */
#ifndef VBAT_LOAD_SETTLE_MS
#define VBAT_LOAD_SETTLE_MS      2U         /* small delay after load enable */
#endif

#ifndef VBAT_LOAD_SAMPLES
#define VBAT_LOAD_SAMPLES        16U        /* min sample count under load */
#endif

/* ===== ADC validity thresholds ===== */
#ifndef VBAT_COUNTS_MIN_VALID
#define VBAT_COUNTS_MIN_VALID    1U
#endif
#ifndef VBAT_COUNTS_MAX_VALID
#define VBAT_COUNTS_MAX_VALID    (ADC_MAX_COUNTS - 1U)
#endif

/* ===== Linear mapping thresholds (battery millivolts) ===== */
#ifndef VBAT_EMPTY_mV
#define VBAT_EMPTY_mV            2800U
#endif
#ifndef VBAT_FULL_mV
#define VBAT_FULL_mV             3600U
#endif

#ifndef VBAT_LEVEL_MIN
#define VBAT_LEVEL_MIN           1U
#endif
#ifndef VBAT_LEVEL_MAX
#define VBAT_LEVEL_MAX           254U
#endif

/* ===== Internal resistance estimate (optional) ===== */
#ifndef VBAT_LOAD_mA
#define VBAT_LOAD_mA             34U
#endif
#ifndef VBAT_RINT_LIMIT_mOHM
#define VBAT_RINT_LIMIT_mOHM     0U         /* 0 = disabled */
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Optional: simple cold compensation (set temp_c if you have a sensor). */
uint16_t vbat_cold_compensation_mv(int16_t temp_c);

/* Measure idle battery in millivolts via gated divider + buffer. */
bool     measure_vbat_idle_mV(ADC_HandleTypeDef *hadc, uint32_t adc_channel, uint16_t *vbat_mv_out);

/* Measure minimum battery voltage under a known load. */
bool     measure_vbat_loaded_min_mV(ADC_HandleTypeDef *hadc, uint32_t adc_channel, uint16_t *vbat_min_mv_out);

/* Map battery (mV + optional temp) to LoRaWAN DevStatusAns Battery (0,1–254,255). */
uint8_t  lorawan_encode_battery(uint16_t vbat_mv,
                                int16_t  temp_c,
                                bool     external_power_present,
                                bool     measurement_ok);

/* Convenience: measure and encode in one call. */
uint8_t  vbat_measure_and_encode(ADC_HandleTypeDef *hadc,
                                 uint32_t adc_channel,
                                 int16_t  temp_c,
                                 bool     external_power_present);

#ifdef __cplusplus
}
#endif
