/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32l0xx_it.c
  * @brief   Interrupt Service Routines.
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

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32l0xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
/* Alarm A fired flag - set in ISR, cleared in main after processing */
volatile bool g_alarm_fired = false;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern RTC_HandleTypeDef hrtc;
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M0+ Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable Interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */
  /*
   * LSE CSS failure triggers NMI on STM32L0.  If the LSE CSS flag is set,
   * handle it here to prevent the program from continuing with a dead LSE
   * (which would hang RTC operations and eventually cause a watchdog reset).
   */
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_LSECSS) != RESET)
  {
    __HAL_RCC_LSECSS_EXTI_CLEAR_FLAG();
    HAL_RCCEx_LSECSS_IRQHandler();
  }
  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
  /* Return immediately for non-CSS NMIs. */
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */
  while (1)
  {
    HAL_NVIC_SystemReset();
  }
  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    HAL_NVIC_SystemReset(); // This is just here as it was auto generated and it makes me feel better
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles System service call via SWI instruction.
  */
void SVC_Handler(void)
{
  /* USER CODE BEGIN SVC_IRQn 0 */

  /* USER CODE END SVC_IRQn 0 */
  /* USER CODE BEGIN SVC_IRQn 1 */

  /* USER CODE END SVC_IRQn 1 */
}

/**
  * @brief This function handles Pendable request for system service.
  */
void PendSV_Handler(void)
{
  /* USER CODE BEGIN PendSV_IRQn 0 */

  /* USER CODE END PendSV_IRQn 0 */
  /* USER CODE BEGIN PendSV_IRQn 1 */

  /* USER CODE END PendSV_IRQn 1 */
}

/**
  * @brief This function handles System tick timer.
  */
void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */

  /* USER CODE END SysTick_IRQn 0 */
  HAL_IncTick();
  /* USER CODE BEGIN SysTick_IRQn 1 */

  /* USER CODE END SysTick_IRQn 1 */
}

/******************************************************************************/
/* STM32L0xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32l0xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles RTC global interrupt through EXTI lines 17, 19 and 20 and LSE CSS interrupt through EXTI line 19.
  */
void RTC_IRQHandler(void)
{
  /* USER CODE BEGIN RTC_IRQn 0 */
  if (__HAL_RCC_LSECSS_EXTI_GET_FLAG() != 0U)
  {
    __HAL_RCC_LSECSS_EXTI_CLEAR_FLAG();
    HAL_RCCEx_LSECSS_IRQHandler();
  }

  /*
   * STM32L0 RTC errata workaround:
   * process/clear all RTC interrupt sources so Alarm A cannot be masked
   * by another RTC source in edge timing conditions.
   */
  for (uint8_t i = 0; i < 3u; ++i)
  {
    if (__HAL_RTC_ALARM_GET_FLAG(&hrtc, RTC_FLAG_ALRAF) != 0U)
    {
      g_alarm_fired = true;
      __HAL_RTC_ALARM_CLEAR_FLAG(&hrtc, RTC_FLAG_ALRAF);
      __HAL_RTC_ALARM_EXTI_CLEAR_FLAG();
    }

    if (__HAL_RTC_WAKEUPTIMER_GET_FLAG(&hrtc, RTC_FLAG_WUTF) != 0U)
    {
      __HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(&hrtc, RTC_FLAG_WUTF);
      __HAL_RTC_WAKEUPTIMER_EXTI_CLEAR_FLAG();
    }

    if (__HAL_RTC_TIMESTAMP_GET_FLAG(&hrtc, RTC_FLAG_TSF) != 0U)
    {
      __HAL_RTC_TIMESTAMP_CLEAR_FLAG(&hrtc, RTC_FLAG_TSF);
      __HAL_RTC_TAMPER_TIMESTAMP_EXTI_CLEAR_FLAG();
    }

#if defined(RTC_FLAG_TAMP1F)
    if (__HAL_RTC_TAMPER_GET_FLAG(&hrtc, RTC_FLAG_TAMP1F) != 0U)
    {
      __HAL_RTC_TAMPER_CLEAR_FLAG(&hrtc, RTC_FLAG_TAMP1F);
      __HAL_RTC_TAMPER_TIMESTAMP_EXTI_CLEAR_FLAG();
    }
#endif
  }
  /* USER CODE END RTC_IRQn 0 */
  HAL_RTC_AlarmIRQHandler(&hrtc);
  /* USER CODE BEGIN RTC_IRQn 1 */

  /* USER CODE END RTC_IRQn 1 */
}

/* USER CODE BEGIN 1 */

/**
  * @brief  RTC Alarm A Event Callback (called from HAL if using HAL_RTC_AlarmIRQHandler)
  * @param  hrtc: RTC handle
  * @retval None
  */
void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc_cb)
{
    (void)hrtc_cb;
    g_alarm_fired = true;
    /* Flags already cleared in RTC_IRQHandler or by HAL; keep ISR minimal */
}

/* USER CODE END 1 */
