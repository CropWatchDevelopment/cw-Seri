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
#include "ezurio_rm126x_at_cmds.h"
#include "errors.h"
#include <stdbool.h>
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
static volatile bool ready_for_tx = false;
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
osTimerId LoRaReplyTimerHandle;
osStaticTimerDef_t LoRaReplyTimerControlBlock;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void startupTaskFunc(void const * argument);
void HealthCheckTaskFunc(void const * argument);
void LoRaTxTaskFunc(void const * argument);
void LoRaReplyTimeout_Cb(void const * argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* GetIdleTaskMemory prototype (linked to static allocation support) */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );
void vApplicationStackOverflowHook( xTaskHandle xTask, signed char *pcTaskName );
/* GetTimerTaskMemory prototype (linked to static allocation support) */
void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize );

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

/* USER CODE BEGIN GET_TIMER_TASK_MEMORY */
static StaticTask_t xTimerTaskTCBBuffer;
static StackType_t xTimerStack[configTIMER_TASK_STACK_DEPTH];

void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize )
{
  *ppxTimerTaskTCBBuffer = &xTimerTaskTCBBuffer;
  *ppxTimerTaskStackBuffer = &xTimerStack[0];
  *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
  /* place for user code */
}
/* USER CODE END GET_TIMER_TASK_MEMORY */

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

  /* Create the timer(s) */
  /* definition and creation of LoRaReplyTimer */
  osTimerStaticDef(LoRaReplyTimer, LoRaReplyTimeout_Cb, &LoRaReplyTimerControlBlock);
  LoRaReplyTimerHandle = osTimerCreate(osTimer(LoRaReplyTimer), osTimerPeriodic, NULL);

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  if (NULL == LoRaReplyTimerHandle) {
      (void)Error_Log(ERROR_CREATING_OS_COMPONENTS);
  }
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* definition and creation of LoRaTxQ */
  osMessageQStaticDef(LoRaTxQ, 4, uint32_t, LoRaTxQBuffer, &LoRaTxInfo);
  LoRaTxQHandle = osMessageCreate(osMessageQ(LoRaTxQ), NULL);
  if (NULL == LoRaTxQHandle) {
      (void)Error_Log(ERROR_CREATING_OS_COMPONENTS);
  }

  /* definition and creation of LoRaRxQ */
  osMessageQStaticDef(LoRaRxQ, 6, uint32_t, LoRaRxQBuffer, &LoRaRxQInfo);
  LoRaRxQHandle = osMessageCreate(osMessageQ(LoRaRxQ), NULL);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  if (NULL == LoRaRxQHandle) {
      (void)Error_Log(ERROR_CREATING_OS_COMPONENTS);
  }
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of startupTask */
  osThreadStaticDef(startupTask, startupTaskFunc, osPriorityNormal, 0, 64, startupTaskBuffer, &startupTaskControlBlock);
  startupTaskHandle = osThreadCreate(osThread(startupTask), NULL);
  if (NULL == startupTaskHandle) {
      (void)Error_Log(ERROR_CREATING_OS_COMPONENTS);
  }

  /* definition and creation of healthCheck */
  osThreadStaticDef(healthCheck, HealthCheckTaskFunc, osPriorityAboveNormal, 0, 64, healthCheckBuffer, &healthCheckControlBlock);
  healthCheckHandle = osThreadCreate(osThread(healthCheck), NULL);
  if (NULL == healthCheckHandle) {
      (void)Error_Log(ERROR_CREATING_OS_COMPONENTS);
  }

  /* definition and creation of LoRaTx */
  osThreadStaticDef(LoRaTx, LoRaTxTaskFunc, osPriorityHigh, 0, 64, LoRaTxBuffer, &LoRaTxControlBlock);
  LoRaTxHandle = osThreadCreate(osThread(LoRaTx), NULL);
  if (NULL == LoRaTxHandle) {
      (void)Error_Log(ERROR_CREATING_OS_COMPONENTS);
  }

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
    static bool initialised = false;
    uint32_t const one_sec_period = 300;

    uint8_t toggle_cmd[sizeof(uint32_t)] = {CMD_TOGGLE_LED, 0x00, 0x00, 0x00};
#if 0
    static AT_Cmd_t at_command_list[] = {
        [0] = {
            .id = 0u, .unsolicited_response = false, .flag = 0x00,
            .mnemonic = EZURIO_RM126x_ATTENTION_MNEMONIC, .length = EZURIO_RM126x_ATTENTION_LENGTH
        },
        [1] = {
            .id = 0u, .unsolicited_response = false, .flag = 0x00,
            .mnemonic = EZURIO_RM126x_GET_DEVICE_NAME_MNEMONIC, .length = EZURIO_RM126x_GET_DEVICE_NAME_LENGTH
        },
    };
#endif // 0
    while (!initialised) {
        osStatus os_err_code = osTimerStart(LoRaReplyTimerHandle, one_sec_period);
        if (osOK == os_err_code) {
            initialised = true;
        }
        else {
            (void)Error_Log(ERROR_STARTING_OS_COMPONENTS);
            (void)osDelay(one_sec_period / 10);
        }
    }

    /* Infinite loop */
    for(;;) {
        static uint32_t just_waiting = 0uL;
        while (!ready_for_tx) {
            //osDelay(1);
            just_waiting++;
        }
        // send a command
        /**
        * @brief Put a Message to a Queue.
        * @param  queue_id  message queue ID obtained with \ref osMessageCreate.
        * @param  info      message information.
        * @param  millisec  timeout value or 0 in case of no time-out.
        * @retval status code that indicates the execution status of the function.
        * @note   MUST REMAIN UNCHANGED: \b osMessagePut shall be consistent in every CMSIS-RTOS.
        */
        osStatus os_status = osMessagePut (LoRaTxQHandle, (uint32_t)toggle_cmd, 0uL); // send without a timeout
        if (osOK == os_status) {
            ready_for_tx = false;
        }
        else {
            (void)Error_Log(ERROR_WRITE_TO_Q);
        }
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
    (void)argument; // consider sending a struct with the queue handle and maybe its buffer, size, etc.
#define QUEUE_ITEM_SIZE sizeof(uint32_t)
    uint8_t rxData[QUEUE_ITEM_SIZE];
    /* USER CODE BEGIN LoRaTxTaskFunc */
    /* Infinite loop */
    for(;;) {
        // Block until data is received
        if (xQueueReceive(LoRaTxQHandle, rxData, portMAX_DELAY) == pdPASS) {
            // Process received data
            if (rxData[0] == CMD_TOGGLE_LED) {
                // toggle Led on PA5.
                HAL_GPIO_TogglePin(DBG_LED_GPIO_Port, DBG_LED_Pin);
            }
            else if (rxData[0] == CMD_SHORT_BEEP) {
            }
            else if (rxData[0] == CMD_LONG_BEEP) {
            }
            else if (rxData[0] == CMD_TX_MSG) {
                // Send the msg over the UART (structs (in a union) must contain the required UART ID, the length and a ptr to the msg.
                // Once placed for Tx via UART, The UART's TX-callback pushes the TX_Seq# onto a Queue.
                // The UART's RX-callback is notified by the UART-RX-ISR, which then notifies the LoRaRxTask about an incoming (Rx) msg,
                // which shall be matched with its corresponding Tx msg. It shall be indicated if an unsolicited response is expected (at any time - async) or not,
                // so the LoRaRxTask knows whether to wait for further data or not.
                // Check with Kevin if an \nOK\r is expected only at the end or not.
            }
            else if (rxData[1] == CMD_TOGGLE_LED) {
                // toggle Led on PAx.
            }
            else if (rxData[1] == CMD_SHORT_BEEP) {
            }
            else if (rxData[1] == CMD_LONG_BEEP) {
            }
            else if (rxData[1] == CMD_TX_MSG) {
            }
            else if (rxData[2] == CMD_TOGGLE_LED) {
                // toggle Led on PBx.
            }
            else if (rxData[2] == CMD_SHORT_BEEP) {
            }
            else if (rxData[2] == CMD_LONG_BEEP) {
            }
            else if (rxData[2] == CMD_TX_MSG) {
            }
            else {
                (void)Error_Log(ERROR_UNKNOWN_CMD);
            }
            // For example: toggle LED, log, etc.
        }
        else {
            (void)Error_Log(ERROR_READ_FROM_Q);
        }
        osDelay(1);
    }
    /* USER CODE END LoRaTxTaskFunc */
}

/* LoRaReplyTimeout_Cb function */
void LoRaReplyTimeout_Cb(void const * argument)
{
    /* USER CODE BEGIN LoRaReplyTimeout_Cb */
    (void)argument;
    ready_for_tx = true;
    /* USER CODE END LoRaReplyTimeout_Cb */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

