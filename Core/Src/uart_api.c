#include "uart_api.h"
#include <stdbool.h>

#define SET_JOB_IN_PROGRESS(uart)    uart_jobs[(uart)].status = UART_JOB_IN_PROGRESS
#define SET_JOB_COMPLETED(uart)      uart_jobs[(uart)].status = UART_JOB_COMPLETE
#define ALLOCATE_JOB_ID(uart)        uart_jobs[(uart)].job_id = Allocate_Uart_Tx_Job_Id()

static UartJob_t uart_jobs[SUPPORTED_UARTS] = {
    [DEBUG_UART] = { .uart_id = DEBUG_UART, .ready = false },
    [LORAWAN_UART] = { .uart_id = LORAWAN_UART, .ready = false }
};

static uint32_t uart_tx_job_id = 0uL;

static uint32_t Allocate_Uart_Tx_Job_Id(void);

static bool Uart_Transmitter(UART_HandleTypeDef *const huart, uint8_t const * const data_p, uint16_t length);

bool DBG_Uart_Transmit(uint8_t const * const data_p, uint16_t length, UartJob_t** status_p)
{
    SET_JOB_IN_PROGRESS(DEBUG_UART);
    ALLOCATE_JOB_ID(DEBUG_UART);

    bool uart_tx_started = Uart_Transmitter(&huart1, data_p, length);
    if (!uart_tx_started) {
        // In most cases the UART Tx ISR will be called before we even get here, i.e. transmission has already been completed.
        uart_jobs[DEBUG_UART].status = UART_JOB_FAILED;
        uart_jobs[DEBUG_UART].ready = false;
    }
    *status_p = &uart_jobs[DEBUG_UART];

    return true;
}

bool LORA_Uart_Transmit(uint8_t const * const data_p, uint16_t length, UartJob_t** status_p)
{
    SET_JOB_IN_PROGRESS(LORAWAN_UART);
    ALLOCATE_JOB_ID(LORAWAN_UART);

    bool uart_tx_started = Uart_Transmitter(&huart2, data_p, length);
    if (!uart_tx_started) {
        // It makes sense that even before we get here, the UART Tx ISR has already completed the transmission! Be careful with the flags being over-written!
        uart_jobs[LORAWAN_UART].status = UART_JOB_FAILED;
        uart_jobs[LORAWAN_UART].ready = false;
    }
    *status_p = &uart_jobs[LORAWAN_UART];

    return true;
}

#ifdef DEBUG
uint32_t GetLatestAllocatedUartTxJobId(void)
{
    return uart_tx_job_id;
}
#endif /* DEBUG */


/**
  * @brief Tx Transfer completed callback.
  * @param huart UART handle.
  * @retval None
  */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == huart1.Instance) {
        uart_jobs[DEBUG_UART].status = UART_JOB_COMPLETE;
        uart_jobs[DEBUG_UART].ready = true;
    }
    else if (huart->Instance == huart2.Instance) {
        uart_jobs[LORAWAN_UART].status = UART_JOB_COMPLETE;
        uart_jobs[LORAWAN_UART].ready = true;
    }
}

/**
  * @brief  Reception Event Callback (Rx event notification called after use of advanced reception service).
  * @param  huart UART handle
  * @param  Size  Number of data bytes available in application reception buffer (indicates a position in
  *               reception buffer until which, data are available)
  * @retval None
  */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(huart);
  UNUSED(Size);

}

/**
  * @brief  Rx Transfer completed callback.
  * @param  huart UART handle.
  * @retval None
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(huart);
}

/**
  * @brief  UART error callback.
  * @param  huart UART handle.
  * @retval None
  */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(huart);
}

/**
  * @brief  UART Abort Complete callback.
  * @param  huart UART handle.
  * @retval None
  */
void HAL_UART_AbortCpltCallback(UART_HandleTypeDef *huart)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(huart);
}


static uint32_t Allocate_Uart_Tx_Job_Id(void)
{
    return ++uart_tx_job_id;
}


static bool Uart_Transmitter(UART_HandleTypeDef *const huart, uint8_t const * const data_p, uint16_t length)
{
    bool success = true;

    if (NULL == data_p) {
         success =  false;
    }
    else if (HAL_UART_Transmit_IT(huart, (uint8_t*)data_p, length) != HAL_OK) {
        success =  false;
    }
    else {
        success = true;
    }
    return success;
}
