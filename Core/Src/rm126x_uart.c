/**
   This file holds definitions and functions describing the behaviour of the RM126x LoRaWAN chip.
*/

/* Includes ------------------------------------------------------------------*/

#include "rm126x_uart.h"
#include "uart_api.h"
#include "stm32l0xx_hal.h"
#include <string.h>
#include <stddef.h>
#include <stdint.h>

/* Private define ------------------------------------------------------------*/

//#define USE_AT_CMD_COLLETION

/* Private macro -------------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

typedef enum collection_process_state_e {
    STATE_INIT = 0,
    STATE_PENDING_FOR_ANY_DATA,
    STATE_DATA_READY,
    STATE_TIMEOUT,
    STATE_ABORT,
    STATE_EXIT,
    PROCESS_STATES

} LoRaWAN_DataCollectionState_t;

/* External variables --------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

static int Wait_for_and_first_stage_response_parsing(UartJob_t *const joinJob_p, uint32_t timeout);
static uint32_t Ticks_to_Timeout_On_Receiving_Data(uint32_t prev_ticks, uint32_t down_counter);
static LoRaWAN_DataCollectionState_t Set_Next_State(LoRaWAN_DataCollectionState_t new_state);
static bool Data_Rx_From_Server_Cb(UartJobStatus_t job_status);

/* Private variables ---------------------------------------------------------*/

static char rm126x_server_response_rx_buffer[BUFFER_SIZE_FOR_SERVER_UNSOLICITED_MSG] = {0};
static LoRaWAN_DataCollectionStatus_t data_collection_status = STATUS_STILL_NOT_AVAILABLE;
static LoRaWAN_DataCollectionState_t data_collection_state = STATE_INIT;

/* Public variables ----------------------------------------------------------*/

#if defined (USE_AT_CMD_COLLETION)
static RM126x_AtCmdSet_t RM126x_AtCmdSet[] = {
    [RM126X_AT_CMD_ID_ATTENTION] = {
        .mnemonic = (char const*) LORAWAN_CMD_ATTENTION_MNEMONIC,
        .tx_length = strlen(LORAWAN_CMD_ATTENTION_MNEMONIC),
        .parameters = 0u,
        .prm_types = NULL,
        .param_delimiter = ' ',
        .successful_reply = LORAWAN_SUCCESS_REPLY_FROM_CHIP,
        .failure_reply = LORAWAN_FAILURE_REPLY_FROM_CHIP,
        .unsolicited_reply = false,
        .flags = 0u
    },
    [RM126X_AT_CMD_ID_BATTERY_REPORT] = {
        .mnemonic = (char const*) LORAWAN_CMD_BATTERY_REPORT_MNEMONIC,
        .tx_length = strlen(LORAWAN_CMD_BATTERY_REPORT_MNEMONIC),
        .parameters = 1u,
        .prm_types = (AtCmdParamType_t const*) & (AtCmdParamType_t) { AT_CMD_PRM_TYPE_NUMERIC },
        .param_delimiter = ' ',
        .successful_reply = LORAWAN_SUCCESS_REPLY_FROM_CHIP,
        .failure_reply = LORAWAN_FAILURE_REPLY_FROM_CHIP,
        .unsolicited_reply = false,
        .flags = 0x81u // just an example for bit-flags which mean something for this command.
    },
    [RM126X_AT_CMD_ID_TEMPERATURE_REPORT] = {
        .mnemonic = (char const*) LORAWAN_CMD_TEMP_MEASURE_MNEMONIC,
        .tx_length = strlen(LORAWAN_CMD_TEMP_MEASURE_MNEMONIC),
        .parameters = 1u,
        .prm_types = (AtCmdParamType_t const*) & (AtCmdParamType_t) { AT_CMD_PRM_TYPE_NUMERIC },
        .param_delimiter = ' ',
        .successful_reply = LORAWAN_SUCCESS_REPLY_FROM_CHIP,
        .failure_reply = LORAWAN_FAILURE_REPLY_FROM_CHIP,
        .unsolicited_reply = false,
        .flags = 0x00
    },
    [RM126X_AT_CMD_ID_REBOOT] = {
        .mnemonic = (char const*) LORAWAN_CMD_TEMP_MEASURE_MNEMONIC,
        .tx_length = strlen(LORAWAN_CMD_TEMP_MEASURE_MNEMONIC),
        .parameters = 0u,
        .prm_types = NULL,
        .param_delimiter = '\0',
        .successful_reply = LORAWAN_SUCCESS_REPLY_FROM_CHIP,
        .failure_reply = LORAWAN_FAILURE_REPLY_FROM_CHIP,
        .unsolicited_reply = false,
        .flags = 0x00
    },
    [RM126X_AT_CMD_ID_WRITE_TO_NVM] = {
        .mnemonic = (char const*) LORAWAN_CMD_WRITE_TO_NVM_MNEMONIC,
        .tx_length = strlen(LORAWAN_CMD_WRITE_TO_NVM_MNEMONIC),
        .parameters = 0u,
        .prm_types = NULL,
        .param_delimiter = '\0',
        .successful_reply = LORAWAN_SUCCESS_REPLY_FROM_CHIP,
        .failure_reply = LORAWAN_FAILURE_REPLY_FROM_CHIP,
        .unsolicited_reply = true,
        .flags = 0x00
    },
    [RM126X_AT_CMD_ID_JOIN] = {
        .mnemonic = (char const*) LORAWAN_CMD_TEMP_MEASURE_MNEMONIC,
        .tx_length = strlen(LORAWAN_CMD_TEMP_MEASURE_MNEMONIC),
        .parameters = 0u,
        .prm_types = NULL,
        .param_delimiter = '\0',
        .successful_reply = LORAWAN_SUCCESS_REPLY_FROM_CHIP,
        .failure_reply = LORAWAN_FAILURE_REPLY_FROM_CHIP,
        .unsolicited_reply = true,
        .flags = 0x42u // just an example for bit-flags which mean something for this command.
    },
    [RM126X_AT_CMD_ID_DROP] = {
        .mnemonic = (char const*) LORAWAN_CMD_DROP_MNEMONIC,
        .tx_length = strlen(LORAWAN_CMD_DROP_MNEMONIC),
        .parameters = 0u,
        .prm_types = NULL,
        .param_delimiter = '\0',
        .successful_reply = LORAWAN_SUCCESS_REPLY_FROM_CHIP,
        .failure_reply = LORAWAN_FAILURE_REPLY_FROM_CHIP,
        .unsolicited_reply = true,
        .flags = 0x00
    },
    [RM126X_AT_CMD_ID_SEND_DATA] = {
        .mnemonic = (char const*) LORAWAN_CMD_SEND_DATA_MNEMONIC,
        .tx_length = strlen(LORAWAN_CMD_SEND_DATA_MNEMONIC),
        .parameters = 1u,
        .prm_types = (AtCmdParamType_t const*) & (AtCmdParamType_t) { AT_CMD_PRM_TYPE_STRING },
        .param_delimiter = ' ',
        .successful_reply = LORAWAN_SUCCESS_REPLY_FROM_CHIP,
        .failure_reply = LORAWAN_FAILURE_REPLY_FROM_CHIP,
        .unsolicited_reply = false,
        .flags = 0x00
    },
};
#endif /* USE_AT_CMD_COLLETION */

/* Public (though not necessarily API) functions -----------------------------*/

/* API functions -------------------------------------------------------------*/

/**
 * @brief   Get the AT-command-based serial device's attention and check if its ready.
 * @details This sends an "AT\r\n" command and expects the string "OK\r\n" is response.
 * @param   none
 * @note    The command is used for checking whether the connected device is ready for its next command.
 * @return  0 upon reception of the expected response, 1 transmission failure, 2 detected error(s),
 * @return  3 indicating characters are still missing, 4 device is busy / not ready, 5 device didn't get the request.
 */
int LoRaWAN_Get_Attention(void)
{
    char attention_rx_buffer[8] = {0};
    UartJob_t* joinJob_p = NULL;
    size_t const expected_rx_lengths = strlen(LORAWAN_SUCCESS_REPLY_FROM_CHIP) - 1;
    bool attention_req_success = LORA_Uart_Expect_Response_For_Tx((uint8_t const * const) LORAWAN_CMD_ATTENTION,
            sizeof(LORAWAN_CMD_ATTENTION), (uint8_t*)attention_rx_buffer, expected_rx_lengths, &joinJob_p);
    int return_code = (attention_req_success) ? Wait_for_and_first_stage_response_parsing(joinJob_p, MAX_TICKS_TO_WAIT_FOR_SHORT_RESPONSE_MSG) : 1;
    return return_code;
}

/**
 * @brief   Send a request for the LoRaWAN device to join a server's network.
 * @details This sends an AT-command and expects a response in the form of a string.
 * @param   none
 * @note    Two responses are expected. An immediate response, indicating successful acceptance of the request by the device,
 *          and a following (delayed) response from the server itself, including details info about the network.
 * @return  0 upon reception of the expected response, 1 transmission failure, 2 detected error(s),
 * @return  3 indicating characters are still missing, 4 device is busy / not ready, 5 device didn't get the request.
 */
int LoRaWAN_Join(void)
{
    char join_rx_buffer[8] = {0};
    UartJob_t* joinJob_p = NULL;
    size_t const expected_rx_lengths = strlen(LORAWAN_SUCCESS_REPLY_FROM_CHIP) - 1;
    bool join_req_success = LORA_Uart_Expect_Response_For_Tx((uint8_t const * const) LORAWAN_CMD_JOIN_MNEMONIC,
            expected_rx_lengths, (uint8_t*)join_rx_buffer, sizeof(join_rx_buffer), &joinJob_p);
    int return_code = (join_req_success) ? Wait_for_and_first_stage_response_parsing(joinJob_p, MAX_TICKS_TO_WAIT_FOR_SHORT_RESPONSE_MSG) : 1;
    return return_code;
}

/**
 * @brief   Send a request for the LoRaWAN device to dis-join, aka "drop" from, a server's network.
 * @details This sends an AT-command and expects a response in the form of a string.
 * @param   none
 * @note    Two responses are expected. An immediate response, indicating successful acceptance of the request by the device,
 *          and a following (delayed) response from the server itself, including details info.
 * @return  0 upon reception of the expected response, 1 transmission failure, 2 detected error(s),
 * @return  3 indicating characters are still missing, 4 device is busy / not ready, 5 device didn't get the request.
 */
int LoRaWAN_Drop(void)
{
    char drop_rx_buffer[8] = {0};
    UartJob_t* joinJob_p = NULL;
    size_t const expected_rx_lengths = strlen(LORAWAN_SUCCESS_REPLY_FROM_CHIP) - 1;
    bool drop_req_success = LORA_Uart_Expect_Response_For_Tx((uint8_t const * const) LORAWAN_CMD_DROP_MNEMONIC,
            strlen(LORAWAN_CMD_DROP), (uint8_t*)drop_rx_buffer, expected_rx_lengths, &joinJob_p);
    int return_code = (drop_req_success) ? Wait_for_and_first_stage_response_parsing(joinJob_p, MAX_TICKS_TO_WAIT_FOR_SHORT_RESPONSE_MSG) : 1;
    return return_code;
}

/**
 * @brief   Send a request for the LoRaWAN device to restart (reboot) itself.
 * @details none
 * @param   none
 * @note    Only a single immediate response is expected.
 * @return  0 upon reception of the expected response, 1 transmission failure, 2 detected error(s),
 * @return  3 indicating characters are still missing, 4 device is busy / not ready, 5 device didn't get the request.
 */
int LoRaWAN_Reboot(void)
{
    char reboot_rx_buffer[8] = {0};
    UartJob_t* joinJob_p = NULL;
    size_t const expected_rx_lengths = strlen(LORAWAN_SUCCESS_REPLY_FROM_CHIP) - 1;
    bool reboot_req_success = LORA_Uart_Expect_Response_For_Tx((uint8_t const * const) LORAWAN_CMD_REBOOT_MNEMONIC,
            strlen(LORAWAN_CMD_REBOOT), (uint8_t*)reboot_rx_buffer, expected_rx_lengths, &joinJob_p);
    int return_code = (reboot_req_success) ? Wait_for_and_first_stage_response_parsing(joinJob_p, MAX_TICKS_TO_WAIT_FOR_SHORT_RESPONSE_MSG) : 1;
    return return_code;
}

/**
 * @brief   Store the current RM126x configuration into its non-volatile memory.
 * @details none
 * @param   none
 * @note    Only a single immediate response is expected.
 * @return  0 upon reception of the expected response, 1 transmission failure, 2 detected error(s),
 * @return  3 indicating characters are still missing, 4 device is busy / not ready, 5 device didn't get the request.
 */
int LoRaWAN_SaveToNVM(void)
{
    char save_rx_buffer[8] = {0};
    UartJob_t* joinJob_p = NULL;
    size_t const expected_rx_lengths = strlen(LORAWAN_CMD_WRITE_TO_NVM_MNEMONIC) - 1;
    bool save_req_success = LORA_Uart_Expect_Response_For_Tx((uint8_t const * const) LORAWAN_CMD_WRITE_TO_NVM_MNEMONIC,
            strlen(LORAWAN_CMD_WRITE_TO_NVM), (uint8_t*)save_rx_buffer, expected_rx_lengths, &joinJob_p);
    int return_code = (save_req_success) ? Wait_for_and_first_stage_response_parsing(joinJob_p, MAX_TICKS_TO_WAIT_FOR_SHORT_RESPONSE_MSG) : 1;
    return return_code;
}

/**
 * @brief   Open the UART receiver and collect incoming data.
 * @details Internally it is managed as a state-machine, therefore it shall be called periodically (or by a separate OS thread)
 *          until the expected data is received in full, or a timeout occurs.
 * @param   timeout - number of system ticks to wait for the expected data to arrive before raising a timeout event.
 * @note    This function is non-blocking. Its state machine performs either zero or maximum one transition per call.
 * @return  0 if server reply is available, 1 if still not available, 2 when timeout occurs, 3 for any failure, 4 if unknown.
 */
int LoRaWAN_Collect_Server_Reply(uint32_t timeout)
{
    static uint32_t ticks_at_start = 0uL;
    static uint32_t count_down_timeout = 0uL;

    UartJob_t* UartJob_p = NULL;
    bool rx_req_success = false;

    switch (data_collection_state) {
        case STATE_INIT:
            count_down_timeout = timeout;
            rx_req_success = LORA_Uart_Receive((uint8_t*) rm126x_server_response_rx_buffer,
                                               (size_t) (BUFFER_SIZE_FOR_SERVER_UNSOLICITED_MSG - 1),
                                               Data_Rx_From_Server_Cb,
                                               &UartJob_p);
            if (rx_req_success) {
                ticks_at_start = HAL_GetTick();
                (void)Set_Next_State(STATE_PENDING_FOR_ANY_DATA);
                data_collection_status = STATUS_STILL_NOT_AVAILABLE;
            }
            else {
                data_collection_status = STATUS_FAILED;
                (void)Set_Next_State(STATE_ABORT);
            }
            break;
        case STATE_PENDING_FOR_ANY_DATA:
            count_down_timeout = Ticks_to_Timeout_On_Receiving_Data(ticks_at_start, count_down_timeout);
            if (count_down_timeout <= 1) {
                count_down_timeout = 0uL;
                data_collection_status = STATUS_TIMEOUT;
            }
            break;
        case STATE_DATA_READY:
            data_collection_status = STATUS_AVAILABLE;
            break;
        case STATE_EXIT:
        case STATE_ABORT:
            data_collection_status = STATUS_FAILED;
            break;
        default:
            data_collection_status = STATUS_UNKNOWN;
    }
    return data_collection_status;
}

/**
 * @brief   Return the current status of data collection from a remote entity.
 * @details none
 * @param   none
 * @note    none
 * @return  the data collection process status.
 */
LoRaWAN_DataCollectionStatus_t LoRaWAN_Get_Data_Collection_Status(void)
{
    LoRaWAN_DataCollectionStatus_t current_status;

         if (STATE_INIT == data_collection_state)                    current_status = STATUS_STILL_NOT_AVAILABLE;
    else if (STATE_PENDING_FOR_ANY_DATA == data_collection_state)    current_status = STATUS_STILL_NOT_AVAILABLE;
    else if (STATE_DATA_READY == data_collection_state)              current_status = STATUS_AVAILABLE;
    else if (STATE_TIMEOUT == data_collection_state)                 current_status = STATUS_TIMEOUT;
    else if (STATE_EXIT == data_collection_state)                    current_status = STATUS_FAILED;
    else if (STATE_ABORT == data_collection_state)                   current_status = STATUS_FAILED;
    else                                                             current_status = STATUS_UNKNOWN;
    return current_status;
}

/******************************************************************************/
/* Private function implementations ------------------------------------------*/
/******************************************************************************/

/**
 * @brief   This is the RM126x-state-machine callback registered at the uart-api for incoming data events.
 * @details none
 * @param   job_status - the current status of the UART job.
 * @note    none
 * @return  true in case this callback shall be un-registered at the end of its processing, or false - for keeping it registered.
 */
static bool Data_Rx_From_Server_Cb(UartJobStatus_t job_status)
{
    //(void)job_status;
    if ((STATE_INIT == data_collection_state) || (STATE_PENDING_FOR_ANY_DATA == data_collection_state)) {
        if (UART_JOB_COMPLETE == job_status) {
            data_collection_state = STATE_DATA_READY;
        }
        else if ((UART_JOB_TX_IN_PROGRESS == job_status) || (UART_JOB_RX_IN_PROGRESS == job_status)) {
            data_collection_state = STATE_ABORT;
        }
        else if ((UART_JOB_TX_FAILED == job_status) || (UART_JOB_RX_FAILED == job_status) ||
                 (UART_JOB_TX_ABORTED == job_status) || (UART_JOB_RX_ABORTED == job_status)) {
            data_collection_state = STATE_ABORT;
        } else {
            data_collection_state = STATE_EXIT;
        }
    }
    return true; // returning false keeps the callback registered.
}

/**
 * @brief   Compute elapsed time since the previous function run and check for a timeout event.
 * @details none
 * @param   prev_ticks - number of system ticks recorded on the previous function run.
 * @param   down_counter - the requested timeout in system ticks.
 * @note    none
 * @return  the system-tick difference, which is the time to still wait, or 0 - indicating a timeout.
 */
static uint32_t Ticks_to_Timeout_On_Receiving_Data(uint32_t prev_ticks, uint32_t down_counter)
{
    register uint32_t ticks_while_pending = HAL_GetTick();
    register uint32_t elapsed_ticks = (ticks_while_pending >= prev_ticks) ? (ticks_while_pending - prev_ticks) : (UINT32_MAX - prev_ticks + ticks_while_pending + 1);
    return (down_counter- elapsed_ticks);
}

/**
 * @brief   Open the UART receiver and collect incoming data.
 * @details Internally it is managed as a state-machine, therefore it shall be called periodically (or by a separate OS thread)
 *          until the expected data is received in full, or a timeout occurs.
 * @param   timeout - number of system ticks to wait for the expected data to arrive before raising a timeout event.
 * @note    This function is non-blocking. Its state machine either performs no transitions or one transition at most.
 * @return  0 if server reply is available, 1 if still not available, 2 when timeout occurs, 3 for any failure.
 */
static int Wait_for_and_first_stage_response_parsing(UartJob_t *const joinJob_p, uint32_t timeout)
{
    int return_code = 0;
    register uint32_t max_ticks_to_wait = timeout;

    while ((max_ticks_to_wait > 0uL) && !joinJob_p->any_errors &&
            ((UART_JOB_TX_IN_PROGRESS == joinJob_p->status)
          || (UART_JOB_RX_IN_PROGRESS == joinJob_p->status)))
    {
        --max_ticks_to_wait;
    }
    if (0uL == max_ticks_to_wait) {
        return_code = 3; // timeout
    } else if (joinJob_p->any_errors) {
        return_code = 2; // detected errors along the way
    } else if (STRING_MATCH_CODE == strncmp((char const*) joinJob_p->store_rx_data_p, LORAWAN_SUCCESS_REPLY_FROM_CHIP, sizeof(LORAWAN_SUCCESS_REPLY_FROM_CHIP))) {
        return_code = 0; // all good.
    } else if (STRING_MATCH_CODE == strncmp((char const*) joinJob_p->store_rx_data_p, LORAWAN_BUSY_REPLY_FROM_CHIP, sizeof(LORAWAN_BUSY_REPLY_FROM_CHIP))) {
        return_code = 4;
    } else if (STRING_MATCH_CODE == strncmp((char const*) joinJob_p->store_rx_data_p, LORAWAN_FAILURE_REPLY_FROM_CHIP, sizeof(LORAWAN_FAILURE_REPLY_FROM_CHIP))) {
        return_code = 5;
    } else {
        return_code = 2;
    }
    return return_code;
}

/**
 * @brief   Set a new state to the data collection process state-machine.
 * @details The actual updating of the new state is done in a critical section (which disables - re-enables all interrupts).
 * @param   new_state - the new state to set.
 * @note    none
 * @return  the previous state of the state-machine.
 */
static LoRaWAN_DataCollectionState_t Set_Next_State(LoRaWAN_DataCollectionState_t new_state)
{
    LoRaWAN_DataCollectionState_t prev_state = data_collection_state;
    if (new_state < PROCESS_STATES) {
        /* Critical Section */
        __disable_irq();
        data_collection_state = new_state;
        __enable_irq();
        /* End of Critical Section */
    }
    return prev_state;
}
