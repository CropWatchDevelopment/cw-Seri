#ifndef LORAWAN_COMMANDS__H
#define LORAWAN_COMMANDS__H

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>

#define LORAWAN_CMD_ATTENTION_MNEMONIC         "AT\r\n"
#define LORAWAN_ATTENTION_CMD_LENGTH           strlen(LORAWAN_CMD_ATTENTION_MNEMONIC)

#define LORAWAN_CMD_JOIN_MNEMONIC              "AT+JOIN\r\n"
#define LORAWAN_JOIN_CMD_LENGTH                strlen(LORAWAN_CMD_JOIN_MNEMONIC)

#define LORAWAN_CMD_TEMP_MEASURE_MNEMONIC      "AT+TEMP\r\n"
#define LORAWAN_TEMP_MEASURE_CMD_LENGTH        strlen(LORAWAN_CMD_TEMP_MEASURE_MNEMONIC)

#define LORAWAN_SHORT_CMD_TX_TIMEOUT_IN_MSEC   (15)
#define LORAWAN_LONG_CMD_TX_TIMEOUT_IN_MSEC    (50)


#ifdef __cplusplus
}
#endif

#endif /* LORAWAN_COMMANDS__H */
