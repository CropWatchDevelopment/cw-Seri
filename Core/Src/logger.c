/*
 * logger.c
 *
 *  Created on: Oct 23, 2025
 *      Author: taduri.fwdev@outlook.com
 */

#include <logger.h>

/*static*/ SysErrorLog_t system_error_log = { 0 };
/*static*/ ProgressLog_t progress_log = { 0 };


bool Log_Error(SysErrId_t const err_id)
{
    if (err_id >= SYSTEM_ERRORS) {
        return false;
    }
    else {
        *(((uint32_t*)&system_error_log) + err_id) += 1uL;
        return true;
    }
}

bool Log_Progress(ProgressLogItemId_t const progress_id)
{
    if (progress_id >= PROGRESS_ITEMS) {
        return false;
    }
    else {
        *(((uint32_t*)&progress_log) + progress_id) += 1uL;
        return true;
    }
}
