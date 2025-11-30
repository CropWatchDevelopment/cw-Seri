/**
 * @file connection_progress.h
 * @brief Declaration of data types and methods used to track the progress and status of the LoRa connection.
 *
 *  Created on: Nov 29, 2025
 *  Authors   : taduri.fwdev@outlook.com
 */

#ifndef PROGRESS__H_
#define PROGRESS__H_

#ifdef __cplusplus
extern "C"
{
#endif

/* Includes ------------------------------------------------------------------*/

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Public constants ----------------------------------------------------------*/

/* Public macros -------------------------------------------------------------*/

#define CONVERT_PROGRESS_EVENT_ID_TO_EVENT_MASK(evtid) (1uL << evtid)

/* Public typedefs -----------------------------------------------------------*/

typedef enum progress_event_ids_e {
    PROGRESS_FIRST_EVENT_ID = 0,
    PROGRESS_EVENT_ID_JOINED = PROGRESS_FIRST_EVENT_ID,
    PROGRESS_EVENT_ID_ALREADY_JOINED,
    PROGRESS_EVENT_ID_DROPPED,
    PROGRESS_EVENT_ID_ALREADY_DROPPED_OR_LOST,
    PROGRESS_EVENT_ID_TIME_SYNC,
    PROGRESS_EVENT_ID_NEXT,

    PROGRESS_EVENT_IDS
} ProgressEventId_t;

typedef uint32_t ProgresBar_t;

/* External variables --------------------------------------------------------*/

/* External function prototypes ----------------------------------------------*/

/* Public function prototypes ------------------------------------------------*/

/* Public variables ----------------------------------------------------------*/

extern ProgresBar_t ProgresBar;

/* Public functions ----------------------------------------------------------*/

/* Public API functions ------------------------------------------------------*/

void Progress_ApiInit(void);
ProgresBar_t Progress_ApiGetStatus(void);
bool Progress_ApiSetEvent(ProgressEventId_t);
bool Progress_ApiClrEvent(ProgressEventId_t);

#ifdef __cplusplus
}
#endif

#endif /* PROGRESS__H_ */
