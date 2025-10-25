/*
 * logger.h
 *
 *  Created on: Oct 23, 2025
 *      Author: taduri.fwdev@outlook.com
 */

#ifndef LOGGER_H_
#define LOGGER_H_

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
    ERROR_INVALID_CMD,
    ERROR_STACK_OVERFLOW,

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
    uint32_t stack_overflows;

    uint32_t generic;
} SysErrorLog_t;


typedef enum progress_log_item_e {
    NORMAL_TICK = 0,
    IDLE_TICK,
    TIMER_STARTS,
    TIMER_EXPIRATION,
    MSGS_ENQUEUED,
    MSGS_DEQUEUED,

    PROGRESS_GENERIC,
    PROGRESS_ITEMS
} ProgressLogItemId_t;

typedef struct progress_log_s {
    uint32_t normal_ticks;
    uint32_t idle_ticks;
    uint32_t timer_starts;
    uint32_t timer_expirations;
    uint32_t msgs_enqueued;
    uint32_t msgs_dequeued;

    uint32_t generic;
} ProgressLog_t;


bool Log_Error(SysErrId_t const err_id);
bool Log_Progress(ProgressLogItemId_t const progress_id);


#ifdef __cplusplus
}
#endif

#endif /* LOGGER_H_ */
