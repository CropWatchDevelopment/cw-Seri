/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l0xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
typedef struct
{
    uint32_t f_lsi_hz;
    uint32_t scale_q16; // (LSI nominal / measured) in Q16.16
    uint8_t valid;
} lsi_cal_t;

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
bool lsi_calibrate_with_lse(lsi_cal_t *out);
uint32_t lsi_seconds_to_wut_reload(const lsi_cal_t *cal, uint32_t seconds);
extern lsi_cal_t g_lsi_cal;
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define ADC_IN0_Pin GPIO_PIN_0
#define ADC_IN0_GPIO_Port GPIOA
#define DBG_LED_Pin GPIO_PIN_5
#define DBG_LED_GPIO_Port GPIOA
#define VBAT_MEAS_EN_Pin GPIO_PIN_0
#define VBAT_MEAS_EN_GPIO_Port GPIOB
#define I2C_ENABLE_Pin GPIO_PIN_5
#define I2C_ENABLE_GPIO_Port GPIOB

// I2C Error Codes
#define I2C_READ_SUCCESS 0
#define I2C_SENSOR_1_MISSING -1
#define I2C_SENSOR_2_MISSING -2
#define I2C_BOTH_SENSORS_MISSING -3
#define I2C_SENSOR_1_READ_FAIL -4
#define I2C_SENSOR_2_READ_FAIL -5
#define I2C_READ_ERROR_TEMP_MISMATCH -6
#define I2C_READ_ERROR_HUMI_MISMATCH -7

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
