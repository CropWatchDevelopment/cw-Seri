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

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
/* User can use this section to tailor USARTx/UARTx instance used and associated 
   resources */
/* Definition for USARTx clock resources */
#define USART_DEV                        USART1
#define USART_LORA                       USART2

#define USART_DEV_CLK_ENABLE()           __HAL_RCC_USART1_CLK_ENABLE()
#define USART_DEV_RX_GPIO_CLK_ENABLE()   __HAL_RCC_GPIOA_CLK_ENABLE()
#define USART_DEV_TX_GPIO_CLK_ENABLE()   __HAL_RCC_GPIOA_CLK_ENABLE()

#define USART_LORA_CLK_ENABLE()          __HAL_RCC_USART2_CLK_ENABLE()
#define USART_LORA_RX_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOA_CLK_ENABLE()
#define USART_LORA_TX_GPIO_CLK_ENABLE()  __HAL_RCC_GPIOA_CLK_ENABLE()


#define USART_DEV_FORCE_RESET()          __HAL_RCC_USART1_FORCE_RESET()
#define USART_DEV_RELEASE_RESET()        __HAL_RCC_USART1_RELEASE_RESET()

#define USART_LORA_FORCE_RESET()         __HAL_RCC_USART2_FORCE_RESET()
#define USART_LORA_RELEASE_RESET()       __HAL_RCC_USART2_RELEASE_RESET()
/* Definition for USARTx Pins */
#define USART_DEV_TX_PIN                 GPIO_PIN_9
#define USART_DEV_TX_GPIO_PORT           GPIOA
#define USART_DEV_TX_AF                  GPIO_AF4_USART1
#define USART_DEV_RX_PIN                 GPIO_PIN_10
#define USART_DEV_RX_GPIO_PORT           GPIOA
#define USART_DEV_RX_AF                  GPIO_AF4_USART1

#define USART_LORA_TX_PIN                GPIO_PIN_2
#define USART_LORA_TX_GPIO_PORT          GPIOA
#define USART_LORA_TX_AF                 GPIO_AF4_USART2
#define USART_LORA_RX_PIN                GPIO_PIN_3
#define USART_LORA_RX_GPIO_PORT          GPIOA
#define USART_LORA_RX_AF                 GPIO_AF4_USART2

/* Definition for USARTx's NVIC */
#define USARTx_IRQn                      USART1_IRQn
#define USARTx_IRQHandler                USART1_IRQHandler

/* Size of Transmission buffer */
#define TXBUFFERSIZE                      (COUNTOF(aTxBuffer) - 1)
/* Size of Reception buffer */
#define RXBUFFERSIZE                      TXBUFFERSIZE

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */
#define COUNTOF(__BUFFER__)   (sizeof(__BUFFER__) / sizeof(*(__BUFFER__)))
/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define ADC_IN0_Pin             GPIO_PIN_0
#define ADC_IN0_GPIO_Port       GPIOA
#define DBG_LED_Pin             GPIO_PIN_5
#define DBG_LED_GPIO_Port       GPIOA
#define VBAT_MEAS_EN_Pin        GPIO_PIN_0
#define VBAT_MEAS_EN_GPIO_Port  GPIOB
#define I2C_ENABLE_Pin          GPIO_PIN_5
#define I2C_ENABLE_GPIO_Port    GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
