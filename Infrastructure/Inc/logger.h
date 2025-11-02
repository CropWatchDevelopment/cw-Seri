/**
 * @file logger.h
 * @brief Declarations of methods and MACROs related a selective logging facility.
 *
 *  Created on: Oct 28, 2025
 *     Authors: taduri.fwdev@outlook.com
 */

#ifndef LOGGER_H_
#define LOGGER_H_

#ifdef __cplusplus
extern "C"
{
#endif

/* Includes ------------------------------------------------------------------*/

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>

/* Public constants ----------------------------------------------------------*/
#define LOGGER_SIMPLE_MECHANISM
#ifndef LOGGER_SIMPLE_MECHANISM
//#define LOGGER_FULL_MECHANISM
#endif

/* Public macros -------------------------------------------------------------*/

#if defined (LOGGER_SIMPLE_MECHANISM)
#define MAX_LOGGED_FILENAME_LENGTH    (16)

#define NUM_OF_ELEMENTS_IN(arr)       ((sizeof(arr) / sizeof(arr[0])))

#else
#define LOG_VERBO_PRINTF              (0)
#define LOG_VERBO_CRITICAL            (1)
#define LOG_VERBO_SEVERE_ERROR        (LOG_VERBO_CRITICAL + 1)
#define LOG_VERBO_HIGHLY_IMPORTANT    (LOG_VERBO_SEVERE_ERROR + 1)
#define LOG_VERBO_ERROR               (LOG_VERBO_HIGHLY_IMPORTANT + 1)
#define LOG_VERBO_WARNING             (LOG_VERBO_ERROR + 1)
#define LOG_VERBO_CALLBACK            (LOG_VERBO_WARNING + 1)
#define LOG_VERBO_BUG_TRACING         (LOG_VERBO_CALLBACK + 1)
#define LOG_VERBO_IMPORTANT           (LOG_VERBO_BUG_TRACING + 1)
#define LOG_VERBO_SIMULATION          (LOG_VERBO_IMPORTANT + 1)
#define LOG_VERBO_PROC_DEBUG          (LOG_VERBO_SIMULATION + 1) // PROC means 'Procedural'
#define LOG_VERBO_PERIODIC_DEBUG      (LOG_VERBO_PROC_DEBUG + 1)
#define LOG_VERBO_COMMUNICATION       (LOG_VERBO_PERIODIC_DEBUG + 1)
#define LOG_VERBO_DEBUG               (LOG_VERBO_COMMUNICATION + 1)
#define LOG_VERBO_NO_TS               (LOG_VERBO_DEBUG + 1)
#define LOG_VERBO_LL_DEBUG            (LOG_VERBO_NO_TS + 1)
#define LOG_VERBO_INFO                (LOG_VERBO_LL_DEBUG + 1)
#define LOG_VERBOSITY_LEVELS          (LOG_VERBO_INFO + 1)

/* REMEMBER:
 * Adding values above requires a name added into VerbosityLevel[ LOG_VERBOSITY_LEVELS ],
 * or else a hard-fault WILL occur.
 */

#define LOG_PRINTF(fmt, ...)          LOG_Print( LOG_VERBO_PRINTF,              false, (fmt), ##__VA_ARGS__)
#define LOG_CRITICAL(fmt, ...)        LOG_Print( LOG_VERBO_CRITICAL,            false, (fmt), ##__VA_ARGS__)
#define LOG_SEVERE_ERROR(fmt, ...)    LOG_Print( LOG_VERBO_SEVERE_ERROR,        false, (fmt), ##__VA_ARGS__)
#define LOG_HIGHLY_IMPORTANT(fmt,...) LOG_Print( LOG_VERBO_HIGHLY_IMPORTANT,    false, (fmt), ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)           LOG_Print( LOG_VERBO_ERROR,               false, (fmt), ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...)         LOG_Print( LOG_VERBO_WARNING,             false, (fmt), ##__VA_ARGS__)
#define LOG_CALLBACK(fmt, ...)        LOG_Print( LOG_VERBO_CALLBACK,            false, (fmt), ##__VA_ARGS__)
#define LOG_BUG_TRACE(fmt, ...)       LOG_Print( LOG_VERBO_BUG_TRACING,         false, (fmt), ##__VA_ARGS__)
#define LOG_IMPORTANT(fmt, ...)       LOG_Print( LOG_VERBO_IMPORTANT,           false, (fmt), ##__VA_ARGS__)
#define LOG_SIMULATION(fmt, ...)      LOG_Print( LOG_VERBO_SIMULATION,          false, (fmt), ##__VA_ARGS__)
#define LOG_PROC_DEBUG(fmt, ...)      LOG_Print( LOG_VERBO_PROC_DEBUG,          false, (fmt), ##__VA_ARGS__)
#define LOG_PERIODIC_DEBUG(fmt, ...)  LOG_Print( LOG_VERBO_PERIODIC_DEBUG,      false, (fmt), ##__VA_ARGS__)
#define LOG_COMMUNICATION(fmt, ...)   LOG_Print( LOG_VERBO_COMMUNICATION,       false, (fmt), ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...)           LOG_Print( LOG_VERBO_DEBUG,               false, (fmt), ##__VA_ARGS__)
#define LOG_NO_TS(fmt, ...)           LOG_Print( LOG_VERBO_NO_TS,               false, (fmt), ##__VA_ARGS__)
#define LOG_LL_DEBUG(fmt, ...)        LOG_Print( LOG_VERBO_LL_DEBUG,            false, (fmt), ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)            LOG_Print( LOG_VERBO_INFO,                false, (fmt), ##__VA_ARGS__)
#define LOG_HEXDUMP(msg, data, len)   LOG_Hexdump(LOG_VERBO_INFO, msg, data, len)

#endif /* LOGGER_SIMPLE_MECHANISM */

/* Public typedefs -----------------------------------------------------------*/

#if defined (LOGGER_SIMPLE_MECHANISM)
#define INT  int8_t
#define UINT uint32_t

typedef enum  system_errors_e {
    ERROR_MEM_ALLOCATIONS = 0,
    ERROR_MEM_RELEASES,
    ERROR_HARD_FAULTS,
    ERROR_TIMEOUTS,
    ERROR_FUNC_FAILURES,
    ERROR_CB_FUNCTION,
    ERROR_CREATING_OS_COMPONENTS,
    ERROR_STARTING_OS_COMPONENTS,
    ERROR_READ_FROM_Q,
    ERROR_WRITE_TO_Q,
    ERROR_SHOULD_NOT_OCCUR,
    ERROR_INVALID_ARG,
    ERROR_UNKNOWN_CMD,
//    ERROR_INVALID_CMD,
    ERROR_STACK_OVERFLOW,

    ERROR_GENERIC,
    SYSTEM_ERRORS
} SysErrId_t;

typedef struct log_error_line_s {
    uint32_t    occurrences;
    char const* func_name;
    uint16_t    lineno;
    bool const  stop_here;
    uint8_t     spare;
} LogErrLine_t;

typedef struct system_error_log_s {
    LogErrLine_t mem_allocations;
    LogErrLine_t mem_releases;
    LogErrLine_t hard_faults;
    LogErrLine_t timeouts;
    LogErrLine_t func_failures;
    LogErrLine_t cb_failures;
    LogErrLine_t os_creations;
    LogErrLine_t os_starts;
    LogErrLine_t qReads;
    LogErrLine_t qWrites;
    LogErrLine_t should_not_occur;
    LogErrLine_t invalid_args;
    LogErrLine_t unknown_cmd;
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
#endif /* LOGGER_SIMPLE_MECHANISM */

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
