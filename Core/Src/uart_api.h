#ifndef UART_API__H
#define UART_API__H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/

#include "stm32l0xx_hal.h"
#include <stdbool.h>

/* Public defines ------------------------------------------------------------*/

#define UART_JOB_DEBUG
//#define USE_UART_JOB_ID_ALLOCATION

/* Public macros -------------------------------------------------------------*/

#define SHORT_TO_LONG_MSG_TIMEOUT_RATIO          (6)
#define MAX_TICKS_TO_WAIT_FOR_SHORT_RESPONSE_MSG (0x91C5uL)
#define MAX_TICKS_TO_WAIT_FOR_LONG_RESPONSE_MSG  (SHORT_TO_LONG_MSG_TIMEOUT_RATIO * MAX_TICKS_TO_WAIT_FOR_SHORT_RESPONSE_MSG)

/* Public types --------------------------------------------------------------*/

typedef enum uart_ids_e {
    DEBUG_UART = 0,
    LORAWAN_UART,
    ERROR_UART,
    SUPPORTED_UARTS = ERROR_UART

} UartId_t;

typedef enum uart_job_status_e {
    UART_JOB_INIT = 0,
    UART_JOB_TX_IN_PROGRESS,
    UART_JOB_RX_IN_PROGRESS,
    UART_JOB_COMPLETE,
    UART_JOB_TX_FAILED,
    UART_JOB_RX_FAILED,
    UART_JOB_ABORTED,
    UART_JOB_TX_ABORTED,
    UART_JOB_RX_ABORTED,
    UART_JOB_UNKNOWN_STATUS,
    UART_JOB_STATUSES

} UartJobStatus_t;

typedef enum uart_job_type_e {
    UART_JOB_TX_RX = 0,
    UART_JOB_TX_ONLY,
    UART_JOB_RX_ONLY,
    UART_JOB_TYPES

} UartJobType_t;

typedef bool (*DataRxFromServer_Cb_t)(UartJobStatus_t);

typedef struct uart_job_s {
    const    UartId_t        uart_id;

    volatile UartJobType_t   type;
    volatile UartJobStatus_t status;
    volatile bool            any_errors;
#if defined (USE_UART_JOB_ID_ALLOCATION)
    volatile int16_t         job_id; // future option
#endif
             uint8_t*        store_rx_data_p;
             size_t          rx_length_to_expect;
    DataRxFromServer_Cb_t    rx_data_callback;

} UartJob_t;

/* External variables --------------------------------------------------------*/

/* Public variables ----------------------------------------------------------*/

/* Public function prototypes ------------------------------------------------*/

/**
  * @brief  Send data over the UART dedicated for debugging and communication with a serial UART terminal.
  * @param  tx_data_p  (I) A pointer to storage of the data.
  * @param  tx_length  (I) number of data bytes to send.
  * @param  status_p   (O) A pointer to the UART Job which handles this transaction.
  *
  * @retval false - on any failure, with the reason being available via the status_p, true otherwise.
  * @note   The completion of reception is indicated via the output parameter 'status_p'.
  */
bool DBG_Uart_Transmit(uint8_t const * const tx_data_p, size_t tx_length, UartJob_t** status_p);

/**
  * @brief  Send data over the UART dedicated for LORA communication.
  * @param  tx_data_p  (I) A pointer to storage of the data.
  * @param  tx_length  (I) number of data bytes to send.
  * @param  status_p   (O) A pointer to the UART Job which handles this transaction.
  *
  * @retval false - on any failure, with the reason being available via the status_p, true otherwise.
  * @note   The completion of reception is indicated via the output parameter 'status_p'.
  */
bool LORA_Uart_Transmit(uint8_t const * const tx_data_p, size_t tx_length, UartJob_t** status_p);

/**
  * @brief  Expect incoming data over the UART dedicated for debugging and communication with a serial UART terminal.
  * @param  rx_data_p  (I) A pointer to storage of the expected data.
  * @param  rx_length  (I) Minimum number of data bytes expected.
  * @param  rx_cb      (I) A callback function (pointer) to register.
  * @param  status_p   (O) A pointer to the UART Job which handles this transaction.
  *
  * @retval false - on any failure, with the reason being available via the status_p, true otherwise.
  * @note   The completion of reception is indicated via the output parameter 'status_p'.
  */
bool DBG_Uart_Receive(uint8_t* rx_data_p, size_t rx_length, DataRxFromServer_Cb_t rx_cb, UartJob_t** status_p);

/**
  * @brief  Expect incoming data over the UART dedicated for LORA communication.
  * @param  rx_data_p  (I) A pointer to storage of the expected data.
  * @param  rx_length  (I) Minimum number of data bytes expected.
  * @param  rx_cb      (I) A callback function (pointer) to register.
  * @param  status_p   (O) A pointer to the UART Job which handles this transaction.
  *
  * @retval false - on any failure, with the reason being available via the status_p.
  * @retval true - when setting-up the receiver had no issues.
  * @note   The completion of reception is indicated via the output parameter 'status_p'.
  */
bool LORA_Uart_Receive(uint8_t* data_p, size_t length, DataRxFromServer_Cb_t rx_cb, UartJob_t** status_p);

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
bool DBG_Uart_Expect_Response_For_Tx (uint8_t const * const tx_data_p, size_t tx_length, uint8_t* rx_data_p, size_t rx_length, UartJob_t** status_p);

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
bool LORA_Uart_Expect_Response_For_Tx(uint8_t const * const tx_data_p, size_t tx_length, uint8_t* rx_data_p, size_t rx_length, UartJob_t** status_p);

#if defined (USE_UART_JOB_ID_ALLOCATION)
/**
  * @brief  Retrieve the latest JobId allocated for the UART transmission job.
  * @param  none.
  *
  * @retval The 32-bit JobId.
  * @note   This is available only while debugging and not in a release build.
  */
uint32_t GetLatestAllocatedUartTxJobId(void);
#endif /* USE_UART_JOB_ID_ALLOCATION */


#ifdef __cplusplus
}
#endif

#endif /* UART_API__H */
