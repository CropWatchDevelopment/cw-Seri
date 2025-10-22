/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
osThreadId startupTaskHandle;
uint32_t startupTaskBuffer[ 64 ];
osStaticThreadDef_t startupTaskControlBlock;
osThreadId healthCheckHandle;
uint32_t healthCheckBuffer[ 64 ];
osStaticThreadDef_t healthCheckControlBlock;
osThreadId LoRaTxHandle;
uint32_t LoRaTxBuffer[ 64 ];
osStaticThreadDef_t LoRaTxControlBlock;
osMessageQId LoRaTxQHandle;
uint8_t LoRaTxQBuffer[ 4 * sizeof( uint32_t ) ];
osStaticMessageQDef_t LoRaTxInfo;
osMessageQId LoRaRxQHandle;
uint8_t LoRaRxQBuffer[ 6 * sizeof( uint32_t ) ];
osStaticMessageQDef_t LoRaRxQInfo;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void startupTaskFunc(void const * argument);
void HealthCheckTaskFunc(void const * argument);
void LoRaTxTaskFunc(void const * argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* GetIdleTaskMemory prototype (linked to static allocation support) */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );
void vApplicationStackOverflowHook( xTaskHandle xTask, signed char *pcTaskName );
/* Hook prototypes */
void vApplicationIdleHook(void);
void vApplicationTickHook(void);

/* USER CODE BEGIN 2 */
__weak void vApplicationIdleHook( void )
{
   /* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
   to 1 in FreeRTOSConfig.h. It will be called on each iteration of the idle
   task. It is essential that code added to this hook function never attempts
   to block in any way (for example, call xQueueReceive() with a block time
   specified, or call vTaskDelay()). If the application makes use of the
   vTaskDelete() API function (as this demo application does) then it is also
   important that vApplicationIdleHook() is permitted to return to its calling
   function, because it is the responsibility of the idle task to clean up
   memory allocated by the kernel to any task that has since been deleted. */
}
/* USER CODE END 2 */

/* USER CODE BEGIN 3 */
void vApplicationTickHook( void )
{
   /* This function will be called by each tick interrupt if
   configUSE_TICK_HOOK is set to 1 in FreeRTOSConfig.h. User code can be
   added here, but the tick hook is called from an interrupt context, so
   code must not attempt to block, and only the interrupt safe FreeRTOS API
   functions can be used (those that end in FromISR()). */
}
/* USER CODE END 3 */

/* USER CODE BEGIN PREPOSTSLEEP */
__weak void PreSleepProcessing(uint32_t *ulExpectedIdleTime)
{
/* place for user code */
}

__weak void PostSleepProcessing(uint32_t *ulExpectedIdleTime)
{
/* place for user code */
}
/* USER CODE END PREPOSTSLEEP */

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
  *ppxIdleTaskStackBuffer = &xIdleStack[0];
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
  /* place for user code */
}

void vApplicationStackOverflowHook( xTaskHandle xTask, signed char *pcTaskName )
{
    (void)xTask;
    (void)pcTaskName;
}
/* USER CODE END GET_IDLE_TASK_MEMORY */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* definition and creation of LoRaTxQ */
  osMessageQStaticDef(LoRaTxQ, 4, uint32_t, LoRaTxQBuffer, &LoRaTxInfo);
  LoRaTxQHandle = osMessageCreate(osMessageQ(LoRaTxQ), NULL);

  /* definition and creation of LoRaRxQ */
  osMessageQStaticDef(LoRaRxQ, 6, uint32_t, LoRaRxQBuffer, &LoRaRxQInfo);
  LoRaRxQHandle = osMessageCreate(osMessageQ(LoRaRxQ), NULL);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of startupTask */
  osThreadStaticDef(startupTask, startupTaskFunc, osPriorityNormal, 0, 64, startupTaskBuffer, &startupTaskControlBlock);
  startupTaskHandle = osThreadCreate(osThread(startupTask), NULL);

  /* definition and creation of healthCheck */
  osThreadStaticDef(healthCheck, HealthCheckTaskFunc, osPriorityAboveNormal, 0, 64, healthCheckBuffer, &healthCheckControlBlock);
  healthCheckHandle = osThreadCreate(osThread(healthCheck), NULL);

  /* definition and creation of LoRaTx */
  osThreadStaticDef(LoRaTx, LoRaTxTaskFunc, osPriorityHigh, 0, 64, LoRaTxBuffer, &LoRaTxControlBlock);
  LoRaTxHandle = osThreadCreate(osThread(LoRaTx), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

}

/* USER CODE BEGIN Header_startupTaskFunc */
/**
  * @brief  Function implementing the startupTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_startupTaskFunc */
void startupTaskFunc(void const * argument)
{
  /* USER CODE BEGIN startupTaskFunc */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END startupTaskFunc */
}

/* USER CODE BEGIN Header_HealthCheckTaskFunc */
/**
* @brief Function implementing the healthCheck thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_HealthCheckTaskFunc */
void HealthCheckTaskFunc(void const * argument)
{
  /* USER CODE BEGIN HealthCheckTaskFunc */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END HealthCheckTaskFunc */
}

/* USER CODE BEGIN Header_LoRaTxTaskFunc */
/**
* @brief Function implementing the LoRaTx thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_LoRaTxTaskFunc */
void LoRaTxTaskFunc(void const * argument)
{
  /* USER CODE BEGIN LoRaTxTaskFunc */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END LoRaTxTaskFunc */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

