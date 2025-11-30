/**
 * @file connection_progress.c
 * @brief Definition and implementation of a selective logging facility.
 *
 *  Created on: NOV 29, 2025
 *  Authors   : taduri.fwdev@outlook.com
 */

/* Includes ------------------------------------------------------------------*/

#include "connection_progress.h"
#include "logger.h"
#include <stdint.h>
#include <stdbool.h>

/* Private define ------------------------------------------------------------*/

/* Private constants----------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

/* External variables --------------------------------------------------------*/

/* External function prototypes ----------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/*static*/ ProgresBar_t ProgresBar = {0uL};

/* Public variables ----------------------------------------------------------*/

/* Public functions ----------------------------------------------------------*/

/* Public API functions ------------------------------------------------------*/

void Progress_ApiInit(void)
{
    ProgresBar = 0uL;
}
// --------------------------------------------------------------------------- /

ProgresBar_t Progress_ApiGetStatus(void)
{
    return ProgresBar;
}
// --------------------------------------------------------------------------- /

bool Progress_ApiSetEvent(ProgressEventId_t ProgressEventId)
{
    bool already_set = (ProgressEventId < PROGRESS_EVENT_IDS) ? ((ProgresBar & CONVERT_PROGRESS_EVENT_ID_TO_EVENT_MASK(ProgressEventId)) == CONVERT_PROGRESS_EVENT_ID_TO_EVENT_MASK(ProgressEventId)) : false;
    ProgresBar |= CONVERT_PROGRESS_EVENT_ID_TO_EVENT_MASK(ProgressEventId);
    return already_set;
}
// --------------------------------------------------------------------------- /

bool Progress_ApiClrEvent(ProgressEventId_t ProgressEventId)
{
    bool already_clr = (ProgressEventId < PROGRESS_EVENT_IDS) ? ((ProgresBar & CONVERT_PROGRESS_EVENT_ID_TO_EVENT_MASK(ProgressEventId)) != CONVERT_PROGRESS_EVENT_ID_TO_EVENT_MASK(ProgressEventId)) : false;
    ProgresBar &= ~(CONVERT_PROGRESS_EVENT_ID_TO_EVENT_MASK(ProgressEventId));
    return already_clr;
}
// --------------------------------------------------------------------------- /

/* Private function implementation -------------------------------------------*/
