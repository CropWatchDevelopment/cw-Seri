/*
 * ezurio_rm126x_at_cmds.h
 *
 *  Created on: Oct 22, 2025
 *      Author: taduri.fwdev@outlook.com
 */

#ifndef EZURIO_RM126X_AT_CMDS_H_
#define EZURIO_RM126X_AT_CMDS_H_


#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/

#include <string.h>

/* Public defines ------------------------------------------------------------*/

#define EZURIO_RM126x_ATTENTION_MNEMONIC        "AT\r\n"
#define EZURIO_RM126x_ATTENTION_LENGTH          strlen(EZURIO_RM126x_ATTENTION_MNEMONIC)

#define EZURIO_RM126x_GET_DEVICE_NAME_MNEMONIC  "AT%s0?\r\n"
#define EZURIO_RM126x_GET_DEVICE_NAME_LENGTH    strlen(EZURIO_RM126x_GET_DEVICE_NAME_MNEMONIC)

/* Public macros -------------------------------------------------------------*/

/* Public types --------------------------------------------------------------*/

typedef struct at_cmd_s {
    uint16_t     id;
    bool         unsolicited_response;
    uint8_t      flag;
    char const*  mnemonic;
    size_t const length;
} AT_Cmd_t;

/* External variables --------------------------------------------------------*/

/* Public variables ----------------------------------------------------------*/

/* Public function prototypes ------------------------------------------------*/

/* Exported constants --------------------------------------------------------*/

/* Exported macro ------------------------------------------------------------*/

/* Exported functions ------------------------------------------------------- */

/**
 * @brief   e.g. put_msg
 * @details e.g. Push a message into the message queue (ring buffer)
 * @param   e.g. (I) msg - message to push
 * @param   e.g. (O) storage_p - pointer to storage
 * @note    e.g. No overflow handling: old messages will be overwritten if buffer full
 * @return  e.g. Message value if available, otherwise NO_MSG
*/

/* API function prototypes ---------------------------------------------------*/
/**
 * @brief   e.g. put_msg
 * @details e.g. Push a message into the message queue (ring buffer)
 * @param   e.g. (I) msg - message to push
 * @param   e.g. (O) storage_p - pointer to storage
 * @note    e.g. No overflow handling: old messages will be overwritten if buffer full
 * @return  e.g. Message value if available, otherwise NO_MSG
*/

#ifdef __cplusplus
}
#endif

#endif /* EZURIO_RM126X_AT_CMDS_H_ */
