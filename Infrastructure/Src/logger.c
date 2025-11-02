/**
 * @file logger.c
 * @brief Definition and implementation of a selective logging facility.
 *
 *  Created on: Oct 28, 2025
 *  Authors   : taduri.fwdev@outlook.com
 */

/* Includes ------------------------------------------------------------------*/

#include "logger.h"
//#include "app_health_report_error_codes.h"
#include <stdio.h>
#include <stdbool.h>
#if defined (LOGGER_FULL_MECHANISM)
#include <stdarg.h>
#else
#include <string.h>
#endif

/* Private define ------------------------------------------------------------*/

/* Private constants----------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

/* External variables --------------------------------------------------------*/

/* External function prototypes ----------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
#if defined (LOGGER_FULL_MECHANISM)
static char const * const VerbosityLevel[ LOG_VERBOSITY_LEVELS ] =
{
    "NONE", "CRITICAL", "SEVERE_ERROR", "HIGHLY_IMPORTANT", "ERROR", "WARNING",
    "CALLBACK", "BUG_TRACE", "IMPORTANT", "SIMULATION", "PROCEDURE_DEBUG",
    "PERIODIC_DEBUG", "COMMS", "DEBUG", "NO_TIMESTAMP", "LOW_LEVEL_DEBUG", "INFO"
};

static TX_MUTEX logger_mutex;

#else

static ProgressLogItemId_t system_progress_log;

static SysErrorLog_t system_error_log = {
    .mem_allocations = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .lineno = 0u,
        .stop_here = true,
        .spare = 0u,
    },
    .mem_releases = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .lineno = 0u,
        .stop_here = true,
        .spare = 0u,
    },
    .hard_faults = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .lineno = 0u,
        .stop_here = true,
        .spare = 0u,
    },
    .timeouts = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .lineno = 0u,
        .stop_here = false,
        .spare = 0u,
    },
    .func_failures = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .lineno = 0u,
        .stop_here = false,
        .spare = 0u,
    },
    .cb_failures = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .lineno = 0u,
        .stop_here = false,
        .spare = 0u,
    },
    .os_creations = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .lineno = 0u,
        .stop_here = true,
        .spare = 0u,
    },
    .os_starts = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .lineno = 0u,
        .stop_here = true,
        .spare = 0u,
    },
    .qReads = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .lineno = 0u,
        .stop_here = false,
        .spare = 0u,
    },
    .qWrites = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .lineno = 0u,
        .stop_here = false,
        .spare = 0u,
    },
    .should_not_occur = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .lineno = 0u,
        .stop_here = true,
        .spare = 0u,
    },
    .invalid_args = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .lineno = 0u,
        .stop_here = true,
        .spare = 0u,
    },
    .unknown_cmd = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .lineno = 0u,
        .stop_here = true,
        .spare = 0u,
    },
#if 0
    .invalid_cmd = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .lineno = 0u,
        .stop_here = true,
        .spare = 0u,
    },
#endif
    .stack_overflow = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .lineno = 0u,
        .stop_here = true,
        .spare = 0u,
    },
    .generic = {
        .occurrences = 0uL,
        .func_name = (char[MAX_LOGGED_FILENAME_LENGTH]){0},
        .lineno = 0u,
        .stop_here = false,
        .spare = 0u,
    },
};

#endif /* DEBUG_PHASE */

/* Public variables ----------------------------------------------------------*/

/* Public functions ----------------------------------------------------------*/

INT LOG_Init( void )
#if defined(LOGGER_SIMPLE_MECHANISM)
{
    (void)memset((void*)&system_progress_log, 0x00, sizeof(ProgressLogItemId_t));

    for (uint16_t i = 0u; i < (sizeof(system_error_log) / sizeof(LogErrLine_t)); i++) {
        (((LogErrLine_t*)&system_error_log) + i)->occurrences = 0uL;
        (((LogErrLine_t*)&system_error_log) + i)->lineno = 0u;
        (void)memset((void* const)((((LogErrLine_t*)&system_error_log) + i)->func_name), '\0', MAX_LOGGED_FILENAME_LENGTH);
    }
    return 0;
}

bool LOG_Error(SysErrId_t const err_id, char const* func_name, uint16_t const lineno)
{
    if (err_id < SYSTEM_ERRORS) {
        (((LogErrLine_t*)&system_error_log) + err_id)->occurrences += 1uL;
        (((LogErrLine_t*)&system_error_log) + err_id)->lineno = lineno;
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

#else
{
    return( (INT)( tx_mutex_create( &logger_mutex, "logger.mtx", TX_INHERIT ) ) );
}
// --------------------------------------------------------------------------- /

INT                                     LOG_Print(
    UINT                  const         LogVerbLvl,
    bool                                HexDump,
    char                  const * const Fmt,
    ...
    )
{
    INT n = (-1);
    RTC_TimeTypeDef   timeNow = { 0 };
    RTC_DateTypeDef   dateNow = { 0 };
    RTC_TimeAndDate_t rtcTimeAndDate = { .rtc_time_p = ADDR_OF( timeNow ), .rtc_date_p = ADDR_OF( dateNow ) };
    UINT rtcErrorCode = GlobalResource_ApiRTCReadTimeAndDateBinFormat( ADDR_OF( rtcTimeAndDate ) );

    if( rtcErrorCode == TX_SUCCESS )
    {
        bool proceedToPrint = (HexDump == true) ? true : (tx_mutex_get(&logger_mutex, MILLISECONDS_TO_TICKS(1000)) == TX_SUCCESS);

        if (proceedToPrint == true)
        {
            if( ( NULL != Fmt ) && ( LogVerbLvl < LOG_VERBOSITY_LEVELS ) )
            {
                va_list ap;

                if( LogVerbLvl != LOG_VERBO_NO_TS )
                {
                    printf( "\n@%02d:%02d:%02d: ", timeNow.Hours, timeNow.Minutes, timeNow.Seconds );
                }

                if( LogVerbLvl > LOG_VERBO_PRINTF )
                {
                    printf( "%s> ", VerbosityLevel[ LogVerbLvl ] );
                }

                va_start( ap, Fmt );
                n = vprintf( Fmt, ap );
                va_end(ap);
                n = 0;
            }

            /* Can't do much anyway if we failed while trying to release the mutex.
               Therefore, ignore the returned status. */
            if (HexDump == false)
            {
                if( TX_SUCCESS != tx_mutex_put(&logger_mutex) )
                {
                    RECORD_A_HEALTH_ISSUE( MQTT_CLIENT_HEALTH_OS_ISSUE, MQTT_CLIENT_HEALTH_OS_COMPONENT_MUTEX_PUT );
                }
            }
        }
    }
    return( n );
}
// --------------------------------------------------------------------------- /

int                                     LOG_Hexdump(
    UINT                  const         LogVerbLvl,
    char                  const *       Message,
    void                  const *       Data,
    size_t                              Length
    )
{
    if( TX_SUCCESS == tx_mutex_get( ADDR_OF( logger_mutex ), MILLISECONDS_TO_TICKS(1000)) )
    {
        static const char hexdigits[] = "0123456789abcdef";
        static       char hexdump_line[ 48 + 1 ];

        (void)LOG_Print( LogVerbLvl, true, Message );
        UINT i = 0u;

        for( size_t n = 0; n < Length; n++ )
        {
            int ch = (int)(((const uint8_t*)Data)[ n ]);
            hexdump_line[i++] = hexdigits[ch / 16];
            hexdump_line[i++] = hexdigits[ch % 16];
            hexdump_line[i++] = ' ';
            if( i >= (sizeof(hexdump_line) - 1) )
            {
                hexdump_line[ i++ ] = 0;
                (void)LOG_Print( LogVerbLvl, true, hexdump_line );
                i = 0u;
            }
        }

        if( i > 0 )
        {
            hexdump_line[ i++ ] = 0;
            (void)LOG_Print( LogVerbLvl, true, hexdump_line );
        }

        /* Can't do much anyway if we failed while trying to release the
         * mutex, so ignoring the status returned. */
        (void)tx_mutex_put( ADDR_OF( logger_mutex ) );
    }

    return (0);
}
#endif /* LOGGER_SIMPLE_MECHANISM */
// --------------------------------------------------------------------------- /

/* Public API functions ------------------------------------------------------*/

/* Private function implementation -------------------------------------------*/

/********************************END OF FILE***********************************/
