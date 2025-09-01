#ifndef UART_API__H
#define UART_API__H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32l0xx_hal.h"
#include <stdbool.h>

typedef enum uart_ids_e {
    DEBUG_UART = 0,
    LORAWAN_UART,
    SUPPORTED_UARTS

} UartId_t;

typedef enum uart_job_status_e {
    UART_JOB_COMPLETE = 0,
    UART_JOB_IN_PROGRESS,
    UART_JOB_FAILED,
    UART_JOB_ABORTED,
    UART_JOB_STATUSES

} UartJobStatus_e;

typedef struct uart_job_s {
    const    UartId_t        uart_id;
    volatile uint16_t        job_id;
    volatile UartJobStatus_e status;
    volatile bool            ready;

} UartJob_t;

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;

bool DBG_Uart_Transmit(uint8_t const * const data_p, uint16_t length, UartJob_t**  status_p);
bool LORA_Uart_Transmit(uint8_t const * const data_p, uint16_t length, UartJob_t** status_p);
#ifdef DEBUG
uint32_t GetLatestAllocatedUartTxJobId(void);
#endif /* DEBUG */


#ifdef __cplusplus
}
#endif

#endif /* UART_API__H */
