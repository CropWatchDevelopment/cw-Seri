#pragma once
/*
 * vbat_lorawan.h
 *
 * Battery measurement + LoRaWAN DevStatusAns battery encoding
 * - Integer-only, STM32 HAL
 * - Assumes 1M:1M divider (×2), buffered, gated by VBAT_MEAS_EN
 *
 * Configure the macros below for your board.
 */

#include <stdint.h>
#include <stdbool.h>
#include "stm32l0xx_hal.h"   /* or your STM32 family header */

/* ===== Board-specific pins (EDIT THESE) ===== */
#ifndef VBAT_MEAS_EN_GPIO_Port
#define VBAT_MEAS_EN_GPIO_Port   GPIOA
#endif

#ifndef VBAT_MEAS_EN_Pin
#define VBAT_MEAS_EN_Pin         GPIO_PIN_1
#endif

/* 1 = VBAT_MEAS_EN high enables measurement; 0 = low enables measurement */
#ifndef VBAT_MEAS_EN_ACTIVE_HIGH
#define VBAT_MEAS_EN_ACTIVE_HIGH 1
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

/* ===== Timing / sampling ===== */
#ifndef VBAT_SETTLE_MS
#define VBAT_SETTLE_MS           4U         /* op-amp + node settle */
#endif

#ifndef VBAT_SAMPLES
#define VBAT_SAMPLES             8U         /* average count */
#endif

/* ===== Piecewise mapping thresholds (battery millivolts) ===== */
#ifndef VBAT_SEG1_MAX_mV
#define VBAT_SEG1_MAX_mV         3600U      /* 3.60 V: top of plateau */
#endif
#ifndef VBAT_SEG1_MIN_mV
#define VBAT_SEG1_MIN_mV         3300U      /* 3.30 V: end plateau */
#endif
#ifndef VBAT_SEG2_MIN_mV
#define VBAT_SEG2_MIN_mV         2800U      /* 2.80 V: start knee */
#endif
#ifndef VBAT_SEG3_MIN_mV
#define VBAT_SEG3_MIN_mV         2000U      /* 2.00 V: datasheet end-of-life */
#endif

/* ===== LoRaWAN scale slices (inclusive) ===== */
#ifndef LORA_SEG1_MIN
#define LORA_SEG1_MIN            200U
#endif
#ifndef LORA_SEG1_MAX
#define LORA_SEG1_MAX            254U
#endif
#ifndef LORA_SEG2_MIN
#define LORA_SEG2_MIN             50U
#endif
#ifndef LORA_SEG2_MAX
#define LORA_SEG2_MAX            199U
#endif
#ifndef LORA_SEG3_MIN
#define LORA_SEG3_MIN              1U
#endif
#ifndef LORA_SEG3_MAX
#define LORA_SEG3_MAX             49U
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Optional: simple cold compensation (set temp_c if you have a sensor). */
uint16_t vbat_cold_compensation_mv(int16_t temp_c);

/* Measure battery in millivolts via gated divider + buffer. */
bool     vbat_read_mv(ADC_HandleTypeDef *hadc, uint32_t adc_channel, uint16_t *vbat_mv_out);

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
