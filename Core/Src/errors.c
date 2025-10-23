/*
 * errors.c
 *
 *  Created on: Oct 23, 2025
 *      Author: taduri.fwdev@outlook.com
 */

#include "errors.h"

/*static*/ SysErrorLog_t system_error_log = { 0 };


bool Error_Log(SysErrId_t const err_id)
{
    if (err_id >= SYSTEM_ERRORS) {
        return false;
    }
    else {
        *((uint32_t*)(&system_error_log + err_id)) += 1uL;
        return true;
    }
}
