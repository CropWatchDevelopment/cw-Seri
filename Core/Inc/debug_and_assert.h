/*
 * debug_and_assert.h
 *
 *  Created on: Nov 6, 2025
 *  Authors   : taduri.fwdev@outlook.com
 */

#ifndef DEBUG_AND_ASSERT_H_
#define DEBUG_AND_ASSERT_H_

#ifdef __cplusplus
extern "C"
{
#endif

/* Includes ------------------------------------------------------------------*/

#include "logger.h"
#include <stdbool.h>

/* Public constants ----------------------------------------------------------*/

/* Public macros -------------------------------------------------------------*/

#if defined (DEBUG_PHASE)
#define DEV_ASSERT_NON_VOID(exp) if (!(exp)) { (void) LOG_ERROR_DETAILS(ERROR_ASSERTION_FAILURE); }
#define ASSERT_MUST_BE_TRUE(exp) if (!(exp)) { (void) LOG_ERROR_DETAILS(ERROR_ASSERTION_FAILURE); }
#else
#define DEV_ASSERT_NON_VOID(exp) if (!(exp)) {}
#define ASSERT_MUST_BE_TRUE(exp) if (!(exp)) {}
#endif

/* Public typedefs -----------------------------------------------------------*/

//-------------------------------------------------------------------------

/* External variables --------------------------------------------------------*/

/* External function prototypes ----------------------------------------------*/

/* Public function prototypes ------------------------------------------------*/

/* Public variables ----------------------------------------------------------*/

/* Public functions ----------------------------------------------------------*/

/* Public API functions ------------------------------------------------------*/


#ifdef __cplusplus
}
#endif

#endif /* DEBUG_AND_ASSERT_H_ */
