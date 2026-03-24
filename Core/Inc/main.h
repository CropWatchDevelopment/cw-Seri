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
/* RTC calendar structure for alarm scheduling (BIN format) */
typedef struct {
    uint8_t year;    /* 0-99 (offset from 2000) */
    uint8_t month;   /* 1-12 */
    uint8_t day;     /* 1-31 */
    uint8_t hours;   /* 0-23 */
    uint8_t minutes; /* 0-59 */
    uint8_t seconds; /* 0-59 */
} rtc_calendar_t;

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
/* RTC Alarm A based scheduling functions */
bool rtc_init_once(void);
void rtc_read_now(rtc_calendar_t *now);
void rtc_compute_next_alarm_fixed_grid(const rtc_calendar_t *now, rtc_calendar_t *next_alarm);
bool rtc_arm_alarm_a(const rtc_calendar_t *alarm_time);

/* Alarm flag - set in ISR, cleared in main loop */
extern volatile bool g_alarm_fired;
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define ADC_IN0_Pin GPIO_PIN_0
#define ADC_IN0_GPIO_Port GPIOA
#define LoRaWAN_TX_Pin GPIO_PIN_2
#define LoRaWAN_TX_GPIO_Port GPIOA
#define LoRaWAN_RX_Pin GPIO_PIN_3
#define LoRaWAN_RX_GPIO_Port GPIOA
#define DBG_LED_Pin GPIO_PIN_5
#define DBG_LED_GPIO_Port GPIOA
#define I2C_ENABLE_Pin GPIO_PIN_5
#define I2C_ENABLE_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
#define DIAG_STAGE_HARDFAULT_SIGNATURE 0xB0F10000u

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
