/** @addtogroup DRAPI_SYS
 * @{
 * The MobiCore system API interface provides system information and system functions to Drivers.
 *
 * <!-- Copyright Giesecke & Devrient GmbH 2009-2012 -->
 */
#ifndef __DRAPIMCSYSTEM_H__
#define __DRAPIMCSYSTEM_H__

#include "DrApiCommon.h"

/** Get information about the implementation of the MobiCore Driver API version.
 *
 * @param drApiVersion     pointer to tlApi version.
 * @return TLAPI_OK if version has been set
 * @returns E_TLAPI_NULL_POINTER    if one parameter is a null pointer.
 */
_DRAPI_EXTERN_C drApiResult_t drApiGetVersion(
    uint32_t *drApiVersion);

/** Terminate the Driver with a exit code.
 * Drivers can use the drApiExit() to terminate themselves and return an exit code. This
 * can be used if the initialization fails or an unrecoverable error occurred. The Driver
 * will be terminated immediately and this function will not return.
 *
 * @param exitCode exit code
 *
 * @return there is no return code, since the function will not return
 */
_DRAPI_EXTERN_C _DRAPI_NORETURN void drApiExit(
    uint32_t    exitCode);

#endif // __DRAPIMCSYSTEM_H__

/** @} */
