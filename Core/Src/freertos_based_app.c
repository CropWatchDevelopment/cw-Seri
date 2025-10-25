/*
 * freertos_based_app.c
 *
 *  Created on: Oct 24, 2025
 *      Author: taduri.fwdev@outlook.com
 */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "logger.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
#define blckqSTACK_SIZE   configMINIMAL_STACK_SIZE

#define TASK_STACK_SPACE_OF_HALF_K_BYTE (64)
#define TASK_STACK_SPACE_OF_1K_BYTE (2 * TASK_STACK_SPACE_OF_HALF_K_BYTE)
#define TASK_STACK_SPACE_OF_2K_BYTE (2 * TASK_STACK_SPACE_OF_1K_BYTE)
#define TASK_STACK_SPACE_OF_3K_BYTE (TASK_STACK_SPACE_OF_2K_BYTE + TASK_STACK_SPACE_OF_1K_BYTE)
#define TASK_STACK_SPACE_OF_4K_BYTE (2 * TASK_STACK_SPACE_OF_2K_BYTE)

#define Q_CONSUMER_TASK_STACK_SIZE     (TASK_STACK_SPACE_OF_1K_BYTE)
#define Q_PRODUCER_TASK_STACK_SIZE     (TASK_STACK_SPACE_OF_1K_BYTE)
#define CONS_PROD_QUEUE_DEPTH          (uint32_t) 1 // messages of 4-BYTE each

/* Private variables ---------------------------------------------------------*/
static uint8_t osQueueBuffer[ CONS_PROD_QUEUE_DEPTH * sizeof( uint32_t ) ];
static osStaticMessageQDef_t osQueueCtrlBlock;

static osThreadId QconsumerTaskHandle;
static uint32_t QconsumerTaskBuffer[ Q_CONSUMER_TASK_STACK_SIZE ];
static osStaticThreadDef_t QconsumerTaskControlBlock;

static osThreadId QproducerTaskHandle;
static uint32_t QproducerTaskBuffer[ Q_PRODUCER_TASK_STACK_SIZE ];
static osStaticThreadDef_t QproducerTaskControlBlock;

static osStaticTimerDef_t TimerControlblock;

/* Public variables ----------------------------------------------------------*/
osMessageQId osQueue;
osTimerId osTimer;
uint32_t ProducerValue = 0uL;
uint32_t ConsumerValue = 0uL;

/* Private function prototypes -----------------------------------------------*/

/* Thread function that creates an incrementing number and posts it on a queue */
static void MessageQueueProducer(const void *argument);

/* Thread function that removes the incrementing number from a queue and checks that
   it is the expected number */
static void MessageQueueConsumer(const void *argument);

static void osTimerCallback(void const *argument);

/* Public functions ----------------------------------------------------------*/

void User_FreeRTOS_Init(void)
{
/* Create the queue used by the two tasks to pass the incrementing number.
   Pass a pointer to the queue in the parameter structure.
 */
  osMessageQStaticDef(osqueue, CONS_PROD_QUEUE_DEPTH, uint32_t, osQueueBuffer, &osQueueCtrlBlock);
  osQueue = osMessageCreate(osMessageQ(osqueue), NULL);
  if (NULL == osQueue) {
      (void)Log_Error(ERROR_CREATING_OS_COMPONENTS);
  }

  /* Note that the producer has a lower priority than the consumer when the tasks are spawned. */
  osThreadStaticDef(QCons, MessageQueueConsumer, osPriorityBelowNormal, 0, blckqSTACK_SIZE, QconsumerTaskBuffer, &QconsumerTaskControlBlock);
  QconsumerTaskHandle = osThreadCreate(osThread(QCons), NULL);
  if (NULL == QconsumerTaskHandle) {
      (void)Log_Error(ERROR_CREATING_OS_COMPONENTS);
  }

  osThreadStaticDef(QProd, MessageQueueProducer, osPriorityLow, 0, blckqSTACK_SIZE, QproducerTaskBuffer, &QproducerTaskControlBlock);
  QproducerTaskHandle = osThreadCreate(osThread(QProd), NULL);
  if (NULL == QproducerTaskHandle) {
      (void)Log_Error(ERROR_CREATING_OS_COMPONENTS);
  }

  /* Create Timer */
  osTimerStaticDef(LEDTimer, osTimerCallback, &TimerControlblock);
  osTimer = osTimerCreate(osTimer(LEDTimer), osTimerPeriodic, NULL);

  return;
}

/* API functions -------------------------------------------------------------*/

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Message Queue Producer Thread.
  * @param  argument: Not used
  * @retval None
  */
static void MessageQueueProducer(const void *argument)
{
    for (;;) {
        if (osMessagePut(osQueue, ProducerValue, 100) != osOK) {
            /* Switch On continuously LED2 to indicate error */
            HAL_GPIO_WritePin(DBG_LED_GPIO_Port, DBG_LED_Pin, GPIO_PIN_SET);
        }
        else {
            /* Increment the variable we are going to post next time round.
               The consumer will expect the numbers to follow in numerical order */
            ++ProducerValue;

            if ((ProducerValue % 10) == 0) {
                if (osOK != osTimerStart(osTimer, 200)) {
                    (void)Log_Error(ERROR_STARTING_OS_COMPONENTS);
                }
                else {
                    (void)Log_Progress(TIMER_STARTS);
                }
            }

            HAL_GPIO_TogglePin(DBG_LED_GPIO_Port, DBG_LED_Pin);
            osDelay(400);
        }
    }
}

/**
  * @brief  Message Queue Consumer Thread.
  * @param  argument: Not used
  * @retval None
  */
static void MessageQueueConsumer(const void *argument)
{
    osEvent event;

    for (;;) {
        /* Get the message from the queue */
        event = osMessageGet(osQueue, 100);

        if (event.status == osEventMessage) {
            if (event.value.v != ConsumerValue) {
                /* Catch-up */
                ConsumerValue = event.value.v;
            }
            else {
                /* Increment the value we expect to remove from the queue next time round */
                ++ConsumerValue;
            }
        }
    }
}

static void osTimerCallback(void const *argument)
{
    (void)argument;
    (void)Log_Progress(TIMER_EXPIRATION);
}

