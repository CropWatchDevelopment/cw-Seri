/*
 * clocks.c
 *
 *  Created on: Oct 21, 2025
 *      Author: taduri.fwdev@outlook.com
 *
 * This file is holds all functionality which is related to clocks and their setting.
 */

/* Includes ------------------------------------------------------------------*/

#include "main.h"
#include "cmsis_os.h"
#include "rtc.h"

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

/* External variables --------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Public (though not necessarily API) functions -----------------------------*/

void HAL_RCCEx_LSECSS_Callback(void)
{
  /*
   * A wakeup is generated in Standby mode. In any other modes, an interrupt can be sent to
   * wake-up the software (see Section 7.3.5 of the reference manual).
   * The software MUST then reset the CSSLSEON bit and stop the defective 32 kHz oscillator
   * by resetting LSEON bit. It can change the RTC clock source (LSI, HSE or no clock) through
   * the RTCSEL bit, or take any required action to secure the application.
   * The frequency of LSE oscillator must be higher than 30 kHz to avoid false positive CSS
   * detection.
   */
   /** Initialise the RCC Oscillators according to the specified parameters
    * in the RCC_OscInitTypeDef structure.
    */
    RCC_OscInitTypeDef RCC_OscInitStruct = {
        .OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_MSI,
        .MSIState = RCC_MSI_ON,
        .LSEState = RCC_LSE_OFF,
        .LSIState = RCC_LSE_ON,
        .HSIState = RCC_HSI_ON,
        .HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT,
        .MSICalibrationValue = 0,
        .MSIClockRange = RCC_MSIRANGE_5,
        .PLL = {
            .PLLState = RCC_PLL_NONE
        }
    };
#warning: "The following might use blocking functions, e.g. when waiting for hardware to sync. Consider doing this in application-code context, rather than ISR."
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
      Error_Handler();
    }
}

/* API functions -------------------------------------------------------------*/

/* Private function implementations ------------------------------------------*/

