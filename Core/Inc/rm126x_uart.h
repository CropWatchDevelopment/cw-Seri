#ifndef RM126X_LORAWAN_UART__H
#define RM126X_LORAWAN_UART__H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Public defines ------------------------------------------------------------*/

/* Public macros -------------------------------------------------------------*/

#define AT_CMD_BAUDRATE                               38400
#define AT_CMD_ENDING                                 "\r\n"

#define STRINGIFY(val)                                #val
#define STR_CONCAT(a, b)                              STRINGIFY(a) STRINGIFY(b)
#define TERMINATE_NUMERIC_WITH_ENDING(val)            STRINGIFY(val) AT_CMD_ENDING
#define TERMINATE_STRING_WITH_ENDING(str)             str AT_CMD_ENDING

#if 0
int main() {
    printf("%s\n", STR_CONCAT(Hello, World)); // Expands to "HelloWorld"
    return 0;
}
#endif

#define LORAWAN_CMD_ATTENTION_MNEMONIC                "AT"
#define LORAWAN_CMD_ATTENTION                         TERMINATE_STRING_WITH_ENDING(LORAWAN_CMD_ATTENTION_MNEMONIC)
// Note there's an additional whitespace in this command, which indicates it shall have a parameter.
#define LORAWAN_CMD_BATTERY_REPORT_MNEMONIC           "AT+BAT "
#define LORAWAN_CMD_BATTERY_REPORT                    TERMINATE_STRING_WITH_ENDING(LORAWAN_CMD_BATTERY_REPORT_MNEMONIC)

#define LORAWAN_CMD_TEMP_MEASURE_MNEMONIC             "AT+TEMP "
#define LORAWAN_CMD_TEMP_MEASURE                      TERMINATE_STRING_WITH_ENDING(LORAWAN_CMD_TEMP_MEASURE_MNEMONIC)

#define LORAWAN_CMD_REBOOT_MNEMONIC                   "ATZ"
#define LORAWAN_CMD_REBOOT                            TERMINATE_STRING_WITH_ENDING(LORAWAN_CMD_REBOOT_MNEMONIC)

#define LORAWAN_CMD_WRITE_TO_NVM_MNEMONIC             "AT&W"
#define LORAWAN_CMD_WRITE_TO_NVM                      TERMINATE_STRING_WITH_ENDING(LORAWAN_CMD_WRITE_TO_NVM_MNEMONIC)

#define LORAWAN_CMD_JOIN_MNEMONIC                     "AT+JOIN"
#define LORAWAN_CMD_JOIN                              STR_TERMINATE_STRING_WITH_ENDING(LORAWAN_CMD_JOIN_MNEMONIC)

#define LORAWAN_CMD_DROP_MNEMONIC                     "AT+DROP"
#define LORAWAN_CMD_DROP                              TERMINATE_STRING_WITH_ENDING(LORAWAN_CMD_DROP_MNEMONIC)

// Note there's an additional whitespace in this command, which indicates it shall have a parameter.
#define LORAWAN_CMD_SEND_DATA_MNEMONIC                "AT+SEND \r\n"

#define LORAWAN_CMD_CONFIG_213_MNEMONIC               "ATS 213=200"
#define LORAWAN_CMD_CONFIG_213                        TERMINATE_STRING_WITH_ENDING(LORAWAN_CMD_CONFIG_213_MNEMONIC)

#define LORAWAN_CMD_CONFIG_SET_BAUDRATE_MNEMONIC      "ATS 302="
#define LORAWAN_CMD_CONFIG_SET_BAUDRATE               LORAWAN_CMD_CONFIG_SET_BAUDRATE_MNEMONIC TERMINATE_NUMERIC_WITH_ENDING(baud)

#define LORAWAN_CMD_CONFIG_602_MNEMONIC               "ATS 602=1"
#define LORAWAN_CMD_CONFIG_602                        TERMINATE_STRING_WITH_ENDING(LORAWAN_CMD_CONFIG_602_MNEMONIC)

#define LORAWAN_CMD_CONFIG_603_MNEMONIC               "ATS 603=0"
#define LORAWAN_CMD_CONFIG_603                        TERMINATE_STRING_WITH_ENDING(LORAWAN_CMD_CONFIG_603_MNEMONIC)

#define LORAWAN_CMD_CONFIG_604_MNEMONIC               "ATS 604=0"
#define LORAWAN_CMD_CONFIG_604                        TERMINATE_STRING_WITH_ENDING(LORAWAN_CMD_CONFIG_604_MNEMONIC)

#define LORAWAN_CMD_CONFIG_611_MNEMONIC               "ATS 611=9"
#define LORAWAN_CMD_CONFIG_611                        TERMINATE_STRING_WITH_ENDING(LORAWAN_CMD_CONFIG_611_MNEMONIC)

#if 0
//AT%S 501="2222222222222222"
//AT%S 502="3333333333333333"
//AT%S 500="AT%S 501="22222222222222223333333333333333" <- This is the past 2 values concatinated together
//ATS 213=2000
//AT&W
//ATZ
#endif // 0

#define LORAWAN_SUCCESS_REPLY_FROM_CHIP        "OK\r\n"
#define LORAWAN_FAILURE_REPLY_FROM_CHIP        "FAIL\r\n"
#define LORAWAN_ERROR_REPLY_FROM_CHIP          "ERROR\r\n"
#define LORAWAN_BUSY_REPLY_FROM_CHIP           "BUSY\r\n"

#define LORAWAN_SHORT_CMD_TX_TIMEOUT_IN_MSEC   (15)
#define LORAWAN_LONG_CMD_TX_TIMEOUT_IN_MSEC    (50)

#define BUFFER_SIZE_FOR_SERVER_UNSOLICITED_MSG (128)

#define STRING_MATCH_CODE                      (0)

/* Public types --------------------------------------------------------------*/

typedef enum rm126x_at_cmd_id_e {
    RM126X_AT_CMD_FIRST_ID = 0,
    RM126X_AT_CMD_ID_ATTENTION = RM126X_AT_CMD_FIRST_ID,
    RM126X_AT_CMD_ID_BATTERY_REPORT,
    RM126X_AT_CMD_ID_TEMPERATURE_REPORT,
    RM126X_AT_CMD_ID_REBOOT,
    RM126X_AT_CMD_ID_WRITE_TO_NVM,
    RM126X_AT_CMD_ID_JOIN,
    RM126X_AT_CMD_ID_DROP,
    RM126X_AT_CMD_ID_SEND_DATA,
    // ...
    RM126X_CONFIGURATION_AT_CMD_ID_START,
    RM126X_CONFIGURATION_AT_CMD_ID_ATS_213 = RM126X_CONFIGURATION_AT_CMD_ID_START,
    RM126X_CONFIGURATION_AT_CMD_ID_ATS_602,
    RM126X_CONFIGURATION_AT_CMD_ID_ATS_603,
    RM126X_CONFIGURATION_AT_CMD_ID_ATS_604,
    RM126X_CONFIGURATION_AT_CMD_ID_ATS_611,
    RM126X_CONFIGURATION_AT_CMD_ID_SET_BAUD,
    RM126X_CONFIGURATION_AT_CMD_ID_ATS_500,
    RM126X_CONFIGURATION_AT_CMD_ID_ATS_501,
    RM126X_CONFIGURATION_AT_CMD_ID_ATS_502,

    RM126X_CONFIGURATION_AT_CMD_ID_END,
    RM126X_AT_CMD_IDS_SUPPORTED = RM126X_CONFIGURATION_AT_CMD_ID_END

} RM126x_AtCmdId_t;

typedef enum at_cmd_parameter_type_e {
    AT_CMD_PRM_TYPE_NONE = 0,
    AT_CMD_PRM_TYPE_NUMERIC = 1,
    AT_CMD_PRM_TYPE_STRING = 2,
    AT_CMD_PRM_TYPE_NUMERIC_OR_STRING = (AT_CMD_PRM_TYPE_NUMERIC | AT_CMD_PRM_TYPE_STRING),
    AT_CMD_PRM_TYPES = 3

} AtCmdParamType_t;

typedef struct rm126x_at_cmd_set_info_s {
    char const*             mnemonic;
    char const*             successful_reply;
    char const*             failure_reply;
    AtCmdParamType_t const* prm_types; // NULL means no parameters.
    size_t                  tx_length;
    uint8_t                 parameters;
    char                    param_delimiter;
    bool                    unsolicited_reply; // true for commands which yield a "delayed" reply, from a remote entity.
    uint8_t                 flags;

} RM126x_AtCmdSet_t;

typedef enum collection_status_e {
    STATUS_AVAILABLE = 0,
    STATUS_STILL_NOT_AVAILABLE,
    STATUS_TIMEOUT,
    STATUS_FAILED,
    STATUS_UNKNOWN,

    COLLECTION_STATUSES

} LoRaWAN_DataCollectionStatus_t;

//extern RM126x_AtCmdSet_t RM126x_AtCmdSet[];

/* External variables --------------------------------------------------------*/

/* Public variables ----------------------------------------------------------*/

/* Public function prototypes ------------------------------------------------*/

/**
 * @brief   Get the AT-command-based serial device's attention and check if its ready.
 * @details This sends an "AT\r\n" command and expects the string "OK\r\n" is response.
 * @param   none
 * @note    The command is used for checking whether the connected device is ready for its next command.
 * @return  0 upon reception of the expected response, 1 transmission failure, 2 detected error(s),
 * @return  3 indicating characters are still missing, 4 device is busy / not ready, 5 device didn't get the request.
 */
int LoRaWAN_Get_Attention(void);

/**
 * @brief   e.g. put_msg
 * @details e.g. Push a message into the message queue (ring buffer)
 * @param   e.g. (I) msg - message to push
 * @param   e.g. (O) storage_p - pointer to storage
 * @note    e.g. No overflow handling: old messages will be overwritten if buffer full
 * @return  e.g. Message value if available, otherwise NO_MSG
 */
int LoRaWAN_Join(void);

/**
 * @brief   e.g. put_msg
 * @details e.g. Push a message into the message queue (ring buffer)
 * @param   e.g. (I) msg - message to push
 * @param   e.g. (O) storage_p - pointer to storage
 * @note    e.g. No overflow handling: old messages will be overwritten if buffer full
 * @return  e.g. Message value if available, otherwise NO_MSG
 */
int LoRaWAN_Drop(void);

/**
 * @brief   Send a request for the LoRaWAN device to restart (reboot) itself.
 * @details none
 * @param   none
 * @note    Only a single immediate responses is expected.
 * @return  0 upon reception of the expected response, 1 transmission failure, 2 detected error(s),
 * @return  3 indicating characters are still missing, 4 device is busy / not ready, 5 device didn't get the request.
 */
int LoRaWAN_Reboot(void);

/**
 * @brief   Store the current RM126x configuration into its non-volatile memory.
 * @details none
 * @param   none
 * @note    Only a single immediate responses is expected.
 * @return  0 upon reception of the expected response, 1 transmission failure, 2 detected error(s),
 * @return  3 indicating characters are still missing, 4 device is busy / not ready, 5 device didn't get the request.
 */
int LoRaWAN_SaveToNVM(void);

/**
 * @brief   Open the UART receiver and collect incoming data.
 * @details Internally it is managed as a state-machine, therefore it shall be called periodically (or by a separate OS thread)
 *          until the expected data is received in full, or a timeout occurs.
 * @param   timeout - number of system ticks to wait for the expected data to arrive before raising a timeout event.
 * @note    This function is non-blocking. Its state machine performs either zero or maximum one transition per call.
 * @return  0 if server reply is available, 1 if still not available, 2 when timeout occurs, 3 for any failure, 4 if unknown.
 */
int LoRaWAN_Collect_Server_Reply(uint32_t timeout);


#if 0
/**
 * @brief   e.g. put_msg
 * @details e.g. Push a message into the message queue (ring buffer)
 * @param   e.g. (I) msg - message to push
 * @param   e.g. (O) storage_p - pointer to storage
 * @note    e.g. No overflow handling: old messages will be overwritten if buffer full
 * @return  e.g. Message value if available, otherwise NO_MSG
 */
int LoRaWAN_Send(void);
#endif // 0

#ifdef __cplusplus
}
#endif

#endif /* RM126X_LORAWAN_UART__H */
