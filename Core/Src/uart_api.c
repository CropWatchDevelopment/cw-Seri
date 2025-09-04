/******************************************************************************/
/* --- #included files -------------------------------------------------------*/

#include "uart_api.h"
#include <stdbool.h>

/* --- Local MACROs ----------------------------------------------------------*/

#define SET_JOB_TX_IN_PROGRESS(uart)    uart_jobs[(uart)].status = UART_JOB_TX_IN_PROGRESS
#define SET_JOB_RX_IN_PROGRESS(uart)    uart_jobs[(uart)].status = UART_JOB_RX_IN_PROGRESS
#define SET_JOB_COMPLETED(uart)         uart_jobs[(uart)].status = UART_JOB_COMPLETE
#define SET_JOB_TX_FAILED(uart)         uart_jobs[(uart)].status = UART_JOB_TX_FAILED
#define SET_JOB_RX_FAILED(uart)         uart_jobs[(uart)].status = UART_JOB_RX_FAILED
#define SET_JOB_TX_ABORTED(uart)        uart_jobs[(uart)].status = UART_JOB_TX_ABORTED
#define SET_JOB_RX_ABORTED(uart)        uart_jobs[(uart)].status = UART_JOB_RX_ABORTED

#if defined (USE_UART_JOB_ID_ALLOCATION)
#define ALLOCATE_JOB_ID(uart)        uart_jobs[(uart)].job_id = Allocate_Uart_Tx_Job_Id()
#define CLEAR_JOB_ID(uart)           uart_jobs[(uart)].job_id = -1

static uint32_t uart_tx_job_id = 0uL;
static uint32_t Allocate_Uart_Tx_Job_Id(void);
#endif

/* --- Local (private) functionality ----------------------------------------*/

static UartJob_t uart_jobs[SUPPORTED_UARTS] = {
    [DEBUG_UART] =   { .uart_id = DEBUG_UART,   .status = UART_JOB_INIT, .store_rx_data_p = NULL, .rx_length_to_expect = 0uL},
    [LORAWAN_UART] = { .uart_id = LORAWAN_UART, .status = UART_JOB_INIT, .store_rx_data_p = NULL, .rx_length_to_expect = 0uL}
};

static bool Uart_Transmitter(UART_HandleTypeDef *const huart, uint8_t const * const data_p, uint16_t length);
static bool Uart_Receiver(UART_HandleTypeDef *const huart, uint8_t* data_p, uint16_t expected_length);
static bool Uart_StartTransmission(UartId_t uart_id, UART_HandleTypeDef *const huart, uint8_t const * const data_p, uint16_t length);
static bool Uart_StartReceiving(UartId_t uart_id, UART_HandleTypeDef *const huart, uint8_t* data_p, uint16_t expected_length);

extern UART_HandleTypeDef     huart1;
extern UART_HandleTypeDef     huart2;

#define DBG_UART_HANDLE     (&huart1)
#define LORAWAN_UART_HANDLE (&huart2)

/******************************************************************************/
/* --- API -------------------------------------------------------------------*/
/******************************************************************************/

/**
  * @brief  Send data over the UART dedicated for debugging and communication with a serial UART terminal.
  * @param  tx_data_p  (I) A pointer to storage of the data.
  * @param  tx_length  (I) number of data bytes to send.
  * @param  status_p   (O) A pointer to the UART Job which handles this transaction.
  *
  * @retval false - on any failure, with the reason being available via the status_p, true otherwise.
  * @note   The completion of reception is indicated via the output parameter 'status_p'.
  */
bool DBG_Uart_Transmit(uint8_t const * const tx_data_p, uint16_t tx_length, UartJob_t** status_p)
{
#if defined (USE_UART_JOB_ID_ALLOCATION)
    CLEAR_JOB_ID(DEBUG_UART);
#endif
    if (NULL != status_p) {
        *status_p = &uart_jobs[DEBUG_UART];
    }
    bool startedOk = Uart_StartTransmission(DEBUG_UART, DBG_UART_HANDLE, tx_data_p, tx_length);
    return startedOk;
}

/**
  * @brief  Expect incoming data over the UART dedicated for debugging and communication with a serial UART terminal.
  * @param  rx_data_p  (I) A pointer to storage of the expected data.
  * @param  rx_length  (I) Minimum number of data bytes expected.
  * @param  status_p   (O) A pointer to the UART Job which handles this transaction.
  *
  * @retval false - on any failure, with the reason being available via the status_p, true otherwise.
  * @note   The completion of reception is indicated via the output parameter 'status_p'.
  */
bool DBG_Uart_Receive(uint8_t *const data_p, uint16_t expected_len, UartJob_t** status_p)
{
    uart_jobs[DEBUG_UART].type = UART_JOB_RX_ONLY;
    if (NULL != status_p) {
        *status_p = &uart_jobs[DEBUG_UART];
    }
    bool rx_started = Uart_StartReceiving(DEBUG_UART, DBG_UART_HANDLE, data_p, expected_len);
    return rx_started;
}

/**
  * @brief  Send data over the UART dedicated for LORA communication.
  * @param  tx_data_p  (I) A pointer to storage of the data.
  * @param  tx_length  (I) number of data bytes to send.
  * @param  status_p   (O) A pointer to the UART Job which handles this transaction.
  *
  * @retval false - on any failure, with the reason being available via the status_p, true otherwise.
  * @note   The completion of reception is indicated via the output parameter 'status_p'.
  */
bool LORA_Uart_Transmit(uint8_t const * const tx_data_p, uint16_t tx_length, UartJob_t** status_p)
{
#if defined (USE_UART_JOB_ID_ALLOCATION)
    CLEAR_JOB_ID(LORAWAN_UART);
#endif
    uart_jobs[LORAWAN_UART].type = UART_JOB_TX_ONLY;
    if (NULL != status_p) {
        *status_p = &uart_jobs[LORAWAN_UART];
    }
    bool startedOk = Uart_StartTransmission(LORAWAN_UART, LORAWAN_UART_HANDLE, tx_data_p, tx_length);
    return startedOk;
}

/**
  * @brief  Expect incoming data over the UART dedicated for LORA communication.
  * @param  rx_data_p  (I) A pointer to storage of the expected data.
  * @param  rx_length  (I) Minimum number of data bytes expected.
  * @param  status_p   (O) A pointer to the UART Job which handles this transaction.
  *
  * @retval false - on any failure, with the reason being available via the status_p, true otherwise.
  * @note   The completion of reception is indicated via the output parameter 'status_p'.
  */
bool LORA_Uart_Receive(uint8_t* rx_data_p, uint16_t rx_length, UartJob_t** status_p)
{
    uart_jobs[LORAWAN_UART].type = UART_JOB_RX_ONLY;
    if (NULL != status_p) {
        *status_p = &uart_jobs[LORAWAN_UART];
    }
    bool rx_started = Uart_StartReceiving(LORAWAN_UART, LORAWAN_UART_HANDLE, rx_data_p, rx_length);
    return rx_started;
}

/**
  * @brief  Send and expect data in response, over the UART dedicated for debugging and communication with a serial UART terminal.
  * @param  tx_data_p  (I) A pointer to the data to be transmitted.
  * @param  tx_length  (I) Number of data bytes available at the transmission buffer.
  * @param  rx_data_p  (I) A pointer to storage of the expected data.
  * @param  rx_length  (I) Minimum number of data bytes expected as part of the response.
  * @param  status_p   (O) A pointer to the UART Job which handles this transaction.
  *
  * @retval false - on any failure, with the reason being available via the status_p, true otherwise.
  * @note   The completion of reception is indicated via the output parameter 'status_p'.
  */
bool DBG_Uart_Expect_Response_For_Tx(uint8_t const * const tx_data_p, uint16_t tx_length, uint8_t* rx_data_p, uint16_t rx_length, UartJob_t** status_p)
{
#if defined (USE_UART_JOB_ID_ALLOCATION)
    ALLOCATE_JOB_ID(DEBUG_UART);
#endif
    uart_jobs[DEBUG_UART].type = UART_JOB_TX_RX;
    uart_jobs[DEBUG_UART].store_rx_data_p = rx_data_p;
    uart_jobs[DEBUG_UART].rx_length_to_expect = rx_length;
    if (NULL != status_p) {
        *status_p = &uart_jobs[DEBUG_UART];
    }
    bool tx_success = Uart_StartTransmission(DEBUG_UART, DBG_UART_HANDLE, tx_data_p, tx_length);
    return tx_success;
}

/**
  * @brief  Send and expect data in response, over the UART dedicated for the LORA connection.
  * @param  tx_data_p  (I) A pointer to the data to be transmitted.
  * @param  tx_length  (I) Number of data bytes available at the transmission buffer.
  * @param  rx_data_p  (I) A pointer to storage of the expected data.
  * @param  rx_length  (I) Minimum number of data bytes expected as part of the response.
  * @param  status_p   (O) A pointer to the UART Job which handles this transaction.
  *
  * @retval false - on any failure, with the reason being available via the status_p, true otherwise.
  * @note   The completion of reception is indicated via the output parameter 'status_p'.
  */
bool LORA_Uart_Expect_Response_For_Tx(uint8_t const * const tx_data_p, uint16_t tx_length, uint8_t* rx_data_p, uint16_t rx_length, UartJob_t** status_p)
{
#if defined (USE_UART_JOB_ID_ALLOCATION)
    ALLOCATE_JOB_ID(LORAWAN_UART);
#endif
    uart_jobs[LORAWAN_UART].type = UART_JOB_TX_RX;
    uart_jobs[LORAWAN_UART].store_rx_data_p = rx_data_p;
    uart_jobs[LORAWAN_UART].rx_length_to_expect = rx_length;
    if (NULL != status_p) {
        *status_p = &uart_jobs[LORAWAN_UART];
    }
    bool tx_success = Uart_StartTransmission(LORAWAN_UART, LORAWAN_UART_HANDLE, tx_data_p, tx_length);
    return tx_success;
}

#ifdef DEBUG
/**
  * @brief  Get a glance of the latest job-Id allocated by the mechanism.
  * @param  none
  * @retval a 32-bit value
  */
#if defined (USE_UART_JOB_ID_ALLOCATION)
uint32_t GetLatestAllocatedUartTxJobId(void)
{
    return uart_tx_job_id;
}
#endif /* USE_UART_JOB_ID_ALLOCATION */
#endif /* DEBUG */


/******************************************************************************/
/* --- Local callbacks -------------------------------------------------------*/
/******************************************************************************/

/**
  * @brief Tx Transfer completed callback.
  * @param huart UART handle.
  * @retval None
  */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    static uint16_t last_path = 0u;

    if (NULL != huart) {
        uint8_t uart_index = (huart->Instance == huart1.Instance) ? DEBUG_UART : (huart->Instance == huart2.Instance) ? LORAWAN_UART : ERROR_UART;
        if (uart_index < ERROR_UART) {
            if ((UART_JOB_TX_RX == uart_jobs[uart_index].type)) {
                if ((NULL != uart_jobs[uart_index].store_rx_data_p) && (uart_jobs[uart_index].rx_length_to_expect > 0uL)) {
                    uart_jobs[uart_index].status == UART_JOB_RX_IN_PROGRESS;

                    bool rx_started = Uart_Receiver(huart, uart_jobs[uart_index].store_rx_data_p, uart_jobs[uart_index].rx_length_to_expect);
                    last_path = 1u;
                    if (!rx_started) {
                        uart_jobs[uart_index].status = UART_JOB_RX_FAILED;
                        last_path = 2u;
                    }
                }
                else {
                    uart_jobs[uart_index].status = UART_JOB_RX_FAILED;
                    last_path = 3u;
                }
            }
            else if (UART_JOB_TX_ONLY == uart_jobs[uart_index].type) {
                uart_jobs[uart_index].status = UART_JOB_COMPLETE;
                last_path = 4u;
            }
            else {
                uart_jobs[uart_index].status = UART_JOB_RX_ABORTED;
                last_path = 5u;
            }
        }
        else {
            uart_jobs[uart_index].status = UART_JOB_RX_FAILED;
            uart_jobs[uart_index].any_errors = true;
            last_path = 6u;
        }
    }
    return;
}

/**
  * @brief  Reception Event Callback (Rx event notification called after use of advanced reception service).
  * @param  huart UART handle
  * @param  Size  Number of data bytes available in application reception buffer (indicates a position in
  *               reception buffer until which, data are available)
  * @retval None
  * @note   Called if we're waiting for the RX line to become idle.
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
  *
  * @note   Called once the expected number of character is received, without any timeout on that number of characters.
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (NULL != huart) {
        uint8_t uart_index = (huart->Instance == huart1.Instance) ? DEBUG_UART : (huart->Instance == huart2.Instance) ? LORAWAN_UART : ERROR_UART;
        if (uart_index < ERROR_UART) {
            uart_jobs[uart_index].status = (HAL_UART_STATE_READY == huart->RxState) ? UART_JOB_COMPLETE : UART_JOB_RX_FAILED;
            uart_jobs[uart_index].any_errors = (HAL_UART_STATE_ERROR == huart->RxState);
        }
        else {
            uart_jobs[uart_index].status = UART_JOB_RX_FAILED;
            uart_jobs[uart_index].any_errors = true;
        }
    }
}

/**
  * @brief  UART error callback.
  * @param  huart UART handle.
  * @retval None
  */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (NULL != huart) {
        uint8_t uart_index = (huart->Instance == huart1.Instance) ? DEBUG_UART : (huart->Instance == huart2.Instance) ? LORAWAN_UART : ERROR_UART;
        if (uart_index < ERROR_UART) {
            uart_jobs[uart_index].status = UART_JOB_RX_FAILED;
            uart_jobs[uart_index].any_errors = (HAL_UART_STATE_ERROR == huart->RxState);
        }
    }
}

/**
  * @brief  UART Abort Complete callback.
  * @param  huart UART handle.
  * @retval None
  */
void HAL_UART_AbortCpltCallback(UART_HandleTypeDef *huart)
{
    if (NULL != huart) {
        uint8_t uart_index = (huart->Instance == huart1.Instance) ? DEBUG_UART : (huart->Instance == huart2.Instance) ? LORAWAN_UART : ERROR_UART;
        if (uart_index < ERROR_UART) {
            uart_jobs[uart_index].status = (UART_JOB_TX_IN_PROGRESS == uart_jobs[uart_index].status)
                                           ? UART_JOB_TX_ABORTED
                                           : (UART_JOB_RX_IN_PROGRESS == uart_jobs[uart_index].status)
                                             ? UART_JOB_RX_ABORTED : UART_JOB_ABORTED;
            uart_jobs[uart_index].any_errors = false;
        }
    }
}


#if defined (USE_UART_JOB_ID_ALLOCATION)
/**
  * @brief  Allocate a unique (sequencial) job identifier to the UART transactions.
  * @param  none
  * @retval a 32-bit value
  */
static uint32_t Allocate_Uart_Tx_Job_Id(void)
{
    return ++uart_tx_job_id;
}
#endif

/**
  * @brief  Activate the UART in interrupt-based reception mode.
  * @param  huart   (I) UART handle.
  * @param  data_p  (I) A pointer to storage of the expected data.
  * @param  length  (I) Minimum number of data bytes expected.
  *
  * @retval false - on any failure or true - otherwise.
  */
static bool Uart_Receiver(UART_HandleTypeDef *const huart, uint8_t* data_p, uint16_t expected_length)
{
    bool success;

    if ((NULL == huart) || (NULL == data_p)) {
         success =  false;
    }
    else if (0uL == expected_length) {
        success = true;
    }
    else {
        __HAL_UART_FLUSH_DRREGISTER(huart);
        __HAL_UART_CLEAR_IDLEFLAG(huart);
        __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF | UART_CLEAR_FEF | UART_CLEAR_PEF | UART_CLEAR_NEF);

        success = (HAL_UART_Receive_IT(huart, data_p, expected_length) == HAL_OK);
    }
    return success;
}

/**
  * @brief  Activate the UART in interrupt-based transmission mode.
  * @param  data_p  (I) A pointer to storage of the data.
  * @param  length  (I) Number of data bytes to transmit.
  *
  * @retval false - on any failure or true - otherwise.
  */
static bool Uart_Transmitter(UART_HandleTypeDef *const huart, uint8_t const * const data_p, uint16_t length)
{
    bool success;

    if ((NULL == huart) || (NULL == data_p)) {
         success =  false;
    }
    else {
        __HAL_UART_FLUSH_DRREGISTER(huart);
        __HAL_UART_CLEAR_IDLEFLAG(huart);
        __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF | UART_CLEAR_FEF | UART_CLEAR_PEF | UART_CLEAR_NEF);

        success = (HAL_UART_Transmit_IT(huart, (uint8_t*)data_p, length) == HAL_OK);
    }
    return success;
}

/**
  * @brief  Start transmission over a UART and manage its job status.
  * @param  uart_id    (I) Internal identifier of a UART.
  * @param  huart      (I) UART handle.
  * @param  data_p     (I) A pointer to the data to be transmitted.
  * @param  length     (I) Number of data bytes available at the transmission buffer.
  * @param  status_p   (O) A pointer to the UART Job which handles this transaction.
  *
  * @retval false - on any failure, with the reason being recorded internally, true otherwise.
  */
static bool Uart_StartTransmission(UartId_t uart_id, UART_HandleTypeDef *const huart, uint8_t const * const data_p, uint16_t length)
{
    // When debugging with breakpoints, in most cases the UART Tx ISR will be called due to an empty tx buffer before we even get here.
    uart_jobs[uart_id].status = UART_JOB_TX_IN_PROGRESS;

    bool uart_tx_started = (DEBUG_UART == uart_id) ? Uart_Transmitter(DBG_UART_HANDLE, data_p, length) : (LORAWAN_UART == uart_id) ? Uart_Transmitter(LORAWAN_UART_HANDLE, data_p, length) : false;

    // When debugging with breakpoints, in most cases the UART Tx ISR will be called due to an empty tx buffer before we even get here.
   if (!uart_tx_started) {
       uart_jobs[uart_id].status = UART_JOB_TX_FAILED;
   }

    return uart_tx_started;
}

/**
  * @brief  Start reception over a UART and manage its job status.
  * @param  uart_id    (I) Internal identifier of a UART.
  * @param  huart      (I) UART handle.
  * @param  data_p     (I) A pointer to storage of the data expected.
  * @param  length     (I) Minumum number of data bytes expected.
  * @param  status_p   (O) A pointer to the UART Job which handles this transaction.
  *
  * @retval false - on any failure, with the reason being recorded internally, true otherwise.
  */
static bool Uart_StartReceiving(UartId_t uart_id, UART_HandleTypeDef *const huart, uint8_t* data_p, uint16_t expected_length)
{
    // When debugging with breakpoints, in most cases the UART Tx ISR will be called due to an empty tx buffer before we even get here.
    if (UART_JOB_RX_ONLY == uart_jobs[uart_id].type) {
        uart_jobs[uart_id].status = UART_JOB_RX_IN_PROGRESS;
    }

    bool uart_rx_started = (DEBUG_UART == uart_id) ? Uart_Receiver(huart, data_p, expected_length) : (LORAWAN_UART == uart_id) ? Uart_Receiver(huart, data_p, expected_length) : false;

    // When debugging with breakpoints, in most cases the UART Tx ISR will be called due to an empty tx buffer before we even get here.
   if (!uart_rx_started) {
       uart_jobs[uart_id].status = UART_JOB_RX_FAILED;
   }

    return uart_rx_started;
}
/******************************************************************************/
