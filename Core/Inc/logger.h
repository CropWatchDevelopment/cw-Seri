/**
 * @file logger.h
 * @brief Declarations of methods and MACROs related a selective logging facility.
 *
 *  Created on: Nov 29, 2025
 *  Authors   : taduri.fwdev@outlook.com
 */

#ifndef LOGGER_H_
#define LOGGER_H_

#ifdef __cplusplus
extern "C"
{
#endif


/*
 * The requirements are:
What do you need to see when you look at the log?
1. Timestamp (Date, e.g. 1/Sept (is year important as well?): RTC + AT+TIME
2. Event: options are:
2a. Reset (with reason)
2b. Connection dropped, i.e. 1. made an attempt to AT+SEND but got an error in response, then 2. issued a AT+TIME or another option to get check the connection but... guess what - no response or received an error, which leads to the conclusion of a "connection dropped".
2c. Connection resumed, i.e. 2b + Issuing AT+DROP and AT+JOIN results in a success.
2d. Re-joining refused / failed, i.e. when attempting to re-join, got no response, or an error.
2e. what else would be considered an event to write into the log?



3. The number of occurrences of each event
4. The number of consecutive occurrences of each event
5. what else would you want to see in the log?



If the doc says: Possible values are as follows:
• 0: Not Connected
• 1: Connected



What would actually be received over uart?
1. OK
0 (or 1)
or
2. OK
0: Not Connected

Kevin Cantrell
6:28 PM
The fiest one

OK
1

Or



OK
0
 */

/* Includes ------------------------------------------------------------------*/

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>

/* Public constants ----------------------------------------------------------*/

/* Public macros -------------------------------------------------------------*/

#define MAX_LOGGED_FILENAME_LENGTH    (24)

/* Public typedefs -----------------------------------------------------------*/

#define INT  int8_t
#define UINT uint32_t

typedef enum  system_errors_e {
    ERROR_MEM_ALLOCATIONS = 0,
    ERROR_MEM_RELEASES,
    ERROR_HARD_FAULTS,
    ERROR_INITIALIZATIONS,
    ERROR_TIMEOUTS,
    ERROR_AT_RESPONSE_TIMEOUTS,
    ERROR_FUNC_FAILURES,
    ERROR_CB_FUNCTION,
    ERROR_CREATING_OS_COMPONENTS,
    ERROR_STARTING_OS_COMPONENTS,
    ERROR_OS_SEMAPHORE_GIVE,
    ERROR_OS_SEMAPHORE_TAKE,
    ERROR_READ_FROM_Q,
    ERROR_WRITE_TO_Q,
    ERROR_I2C_COMMS,
    ERROR_NO_SENSORS_FOUND,
    ERROR_TEMPERATURE_DIFF_TOO_LARGE,
    ERROR_HUMIDITY_DIFF_TOO_LARGE,
    ERROR_TEMPERATURE_TOO_FAR_FROM_AVERAGE,
    ERROR_HUMIDITY_TOO_FAR_FROM_AVERAGE,
    ERROR_ASSERTION_FAILURE,
    ERROR_SHOULD_NOT_OCCUR,
    ERROR_INVALID_ARG,
    ERROR_UNKNOWN_CMD,
    ERROR_UNKNOWN_STATE,
    ERROR_SM_GOT_EVENT_TO_WHICH_IT_HAS_NO_HANDLER,
//    ERROR_INVALID_CMD,
    ERROR_STACK_OVERFLOW,

    ERROR_GENERIC,
    SYSTEM_ERRORS
} SysErrId_t;

typedef struct log_error_line_s {
    uint32_t    occurrences;
    char const* func_name;
    uint16_t    linenum;
    bool const  stop_here;
    uint8_t     spare;
} LogErrLine_t;

typedef struct system_error_log_s {
    LogErrLine_t mem_allocations;
    LogErrLine_t mem_releases;
    LogErrLine_t hard_faults;
    LogErrLine_t initializations;
    LogErrLine_t timeouts;
    LogErrLine_t at_response_timeouts;
    LogErrLine_t func_failures;
    LogErrLine_t cb_failures;
    LogErrLine_t os_creations;
    LogErrLine_t os_starts;
    LogErrLine_t os_sem_gives;
    LogErrLine_t os_sem_takes;
    LogErrLine_t qReads;
    LogErrLine_t qWrites;
    LogErrLine_t i2c_communication;
    LogErrLine_t no_sensors_found;
    LogErrLine_t temperature_diff_between_sensors_is_too_large;
    LogErrLine_t humidity_diff_between_sensors_is_too_large;
    LogErrLine_t temperature_too_far_from_running_average;
    LogErrLine_t humidity_diff_too_far_from_running_average;
    LogErrLine_t assertions;
    LogErrLine_t should_not_occur;
    LogErrLine_t invalid_args;
    LogErrLine_t unknown_cmd;
    LogErrLine_t unknown_state;
    LogErrLine_t sm_got_event_it_has_no_handler_for;
//    LogErrLine_t invalid_cmd;
    LogErrLine_t stack_overflow;

    LogErrLine_t generic;
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

/* External variables --------------------------------------------------------*/

/* External function prototypes ----------------------------------------------*/

/* Public function prototypes ------------------------------------------------*/

/* Public variables ----------------------------------------------------------*/

/* Public functions ----------------------------------------------------------*/

/**
 * @brief Initialize the application-level logger facility.
 * @returns TX_SUCCESS if successful, otherwise the OS failure of creating a mutex.
 */
INT LOG_Init(void);

#if defined(LOGGER_FULL_MECHANISM)

/**
 * @brief Send a formatted message to the log.
 * @param LogVerbLvl: The verbosity level.
 * @param HexDump: A flag indicating whether a Hex-Dump routine is the caller of this routine.
 * @param Fmt: A formatted message to log.
 * @returns TX_SUCCESS if successful, otherwise and OS-error code to reflect the failure.
 */
INT                                     LOG_Print(
    UINT                  const         LogVerbLvl,
    bool                                HexDump,
    char                  const * const Fmt,
    ...
);

/**
 * @brief Dump some hex data into a buffer and send it to the application-logger facility.
 * @param LogVerbLvl: The verbosity level.
 * @param Message: Some text to be logged prior to the hex-data.
 * @param Data: The hex-data to be logged.
 * @param Length: The number of hex bytes to be logged.
 * @returns Zero, indicating success.
 */
INT                                     LOG_Hexdump(
    UINT                                LogVerbLvl,
    char                  const *       Message,
    void                  const *       Data,
    size_t                              Length
);

/* Public API functions ------------------------------------------------------*/

#else
bool LOG_Error(SysErrId_t const err_id, char const* func_name, uint16_t const lineno);
bool LOG_Progress(ProgressLogItemId_t const progress_id);
#define LOGGER_IS_ACTIVE
#if defined (LOGGER_IS_ACTIVE)
#define LOG_ERROR_DETAILS(err)        LOG_Error((err), __func__, __LINE__)
#else
#define LOG_ERROR_DETAILS(err)        (0)
#endif

#endif /* LOGGER_FULL_MECHANISM */

#ifdef __cplusplus
}
#endif

#endif /* LOGGER_H_ */
