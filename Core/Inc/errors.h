/*
 * errors.h
 *
 *  Created on: Oct 23, 2025
 *      Author: taduri.fwdev@outlook.com
 */

#ifndef ERRORS_H_
#define ERRORS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

typedef enum  system_errors_e {
    ERROR_TIMEOUTS = 0,
    ERROR_FUNC_FAILURES,
    ERROR_CB_FUNCTION,
    ERROR_CREATING_OS_COMPONENTS,
    ERROR_STARTING_OS_COMPONENTS,
    ERROR_READ_FROM_Q,
    ERROR_WRITE_TO_Q,
    ERROR_SHOULD_NOT_OCCUR,
    ERROR_INVALID_ARG,
    ERROR_UNKNOWN_CMD,
    ERROR_CMD_INVALID,

    ERROR_GENERIC,
    SYSTEM_ERRORS
} SysErrId_t;

typedef struct system_errors_s {
    uint32_t timeouts;
    uint32_t func_failures;
    uint32_t cb_failures;
    uint32_t os_creations;
    uint32_t os_starts;
    uint32_t qReads;
    uint32_t qWrites;
    uint32_t should_not_occur;
    uint32_t invalid_args;
    uint32_t unknown_cmd;
    uint32_t invalid_cmd;

    uint32_t generic;
} SysErrorLog_t;


bool Error_Log(SysErrId_t const err_id);


#ifdef __cplusplus
}
#endif

#endif /* ERRORS_H_ */
