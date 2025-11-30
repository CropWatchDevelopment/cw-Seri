/**
 * @file logger.c
 * @brief Definition and implementation of a selective logging facility.
 *
 *  Created on: Nov 29, 2025
 *  Authors   : taduri.fwdev@outlook.com
 */

/* Includes ------------------------------------------------------------------*/

#include "logger.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

/* Private define ------------------------------------------------------------*/

/* Private constants----------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

/* External variables --------------------------------------------------------*/

/* External function prototypes ----------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

static ProgressLog_t system_progress_log;

static SysErrorLog_t system_error_log = {
    .mem_allocations = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .linenum = 0u,
        .stop_here = true,
        .spare = 0u,
    },
    .mem_releases = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .linenum = 0u,
        .stop_here = true,
        .spare = 0u,
    },
    .hard_faults = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .linenum = 0u,
        .stop_here = true,
        .spare = 0u,
    },
    .initializations = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .linenum = 0u,
        .stop_here = true,
        .spare = 0u,
    },
    .timeouts = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .linenum = 0u,
        .stop_here = false,
        .spare = 0u,
    },
    .at_response_timeouts = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .linenum = 0u,
        .stop_here = false,
        .spare = 0u,
    },
    .func_failures = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .linenum = 0u,
        .stop_here = false,
        .spare = 0u,
    },
    .cb_failures = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .linenum = 0u,
        .stop_here = false,
        .spare = 0u,
    },
    .os_creations = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .linenum = 0u,
        .stop_here = true,
        .spare = 0u,
    },
    .os_starts = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .linenum = 0u,
        .stop_here = true,
        .spare = 0u,
    },
    .os_sem_gives = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .linenum = 0u,
        .stop_here = true,
        .spare = 0u,
    },
    .os_sem_takes = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .linenum = 0u,
        .stop_here = true,
        .spare = 0u,
    },
    .qReads = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .linenum = 0u,
        .stop_here = false,
        .spare = 0u,
    },
    .qWrites = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .linenum = 0u,
        .stop_here = false,
        .spare = 0u,
    },
    .i2c_communication = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .linenum = 0u,
        .stop_here = false,
        .spare = 0u,
    },
    .no_sensors_found = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .linenum = 0u,
        .stop_here = false,
        .spare = 0u,
    },
    .temperature_diff_between_sensors_is_too_large = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .linenum = 0u,
        .stop_here = false,
        .spare = 0u,
    },
    .humidity_diff_between_sensors_is_too_large = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .linenum = 0u,
        .stop_here = false,
        .spare = 0u,
    },
    .temperature_too_far_from_running_average = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .linenum = 0u,
        .stop_here = false,
        .spare = 0u,
    },
    .humidity_diff_too_far_from_running_average = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .linenum = 0u,
        .stop_here = false,
        .spare = 0u,
    },
    .assertions = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .linenum = 0u,
        .stop_here = false,
        .spare = 0u,
    },
    .should_not_occur = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .linenum = 0u,
        .stop_here = true,
        .spare = 0u,
    },
    .invalid_args = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .linenum = 0u,
        .stop_here = true,
        .spare = 0u,
    },
    .unknown_cmd = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .linenum = 0u,
        .stop_here = true,
        .spare = 0u,
    },
    .unknown_state = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .linenum = 0u,
        .stop_here = true,
        .spare = 0u,
    },
    .sm_got_event_it_has_no_handler_for = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .linenum = 0u,
        .stop_here = true,
        .spare = 0u,
    },
#if 0
    .invalid_cmd = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .linenum = 0u,
        .stop_here = true,
        .spare = 0u,
    },
#endif
    .stack_overflow = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .linenum = 0u,
        .stop_here = true,
        .spare = 0u,
    },
    .generic = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .linenum = 0u,
        .stop_here = false,
        .spare = 0u,
    },
};

/* Public variables ----------------------------------------------------------*/

/* Public functions ----------------------------------------------------------*/

INT LOG_Init( void )
{
    (void)memset((void*)&system_progress_log, 0x00, sizeof(ProgressLogItemId_t));

    for (uint16_t i = 0u; i < (sizeof(system_error_log) / sizeof(LogErrLine_t)); i++) {
        (((LogErrLine_t*)&system_error_log) + i)->occurrences = 0uL;
        (((LogErrLine_t*)&system_error_log) + i)->linenum = 0u;
        (void)memset((void* const)((((LogErrLine_t*)&system_error_log) + i)->func_name), '\0', MAX_LOGGED_FILENAME_LENGTH);
    }
    return 0;
}

bool LOG_Error(SysErrId_t const err_id, char const* func_name, uint16_t const lineno)
{
    if (err_id < SYSTEM_ERRORS) {
        (((LogErrLine_t*)&system_error_log) + err_id)->occurrences += 1uL;
        (((LogErrLine_t*)&system_error_log) + err_id)->linenum = lineno;
        (void)strncpy((char* const)((((LogErrLine_t*)&system_error_log) + err_id)->func_name), func_name, MAX_LOGGED_FILENAME_LENGTH);

        return (((LogErrLine_t*)&system_error_log) + err_id)->stop_here;
    }
    else {
        return false;
    }
}

bool LOG_Progress(ProgressLogItemId_t const progress_id)
{
    if (progress_id >= PROGRESS_ITEMS) {
        return false;
    }
    else {
        *(((uint32_t*)&system_progress_log) + progress_id) += 1uL;
        return true;
    }
}

/* Public API functions ------------------------------------------------------*/

/* Private function implementation -------------------------------------------*/

