/** @addtogroup TLAPI_SYS
 * @{
 * The MobiCore system API interface provides system information and system functions to Trustlets.
 *
 * <!-- Copyright Giesecke & Devrient GmbH 2009-2012 -->
 */
#ifndef __TLAPIMCSYSTEM_H__
#define __TLAPIMCSYSTEM_H__

#include "TlApi/TlApiCommon.h"
#include "TlApi/TlApiError.h"

#include "mcSuid.h"
#include "mcVersionInfo.h"
#include "TlApi/version.h"


/** Platform information structure.
 * The platform information structure returns manufacturer specific information about
 * the hardware platform.
 */
typedef struct {
    uint32_t size;                      /**< size of the structure. */
    uint32_t manufacturerdId;           /**< Manufacturer ID provided by G&D. */
    uint32_t platformVersion;           /**< Version of platform. */
    uint32_t platformInfoDataLenght;    /**< Length of manufacturer specific platform information. */
    uint8_t  platformInfoData[];        /**< Manufacturer specific platform information data. */
} tlApiPlatformInfo_t;

/** Get information about the implementation of the MobiCore Trustlet API version.
 *
 * @param tlApiVersion     pointer to tlApi version.
 * @return TLAPI_OK if version has been set
 * @returns E_TLAPI_NULL_POINTER    if one parameter is a null pointer.
 */
_TLAPI_EXTERN_C tlApiResult_t tlApiGetVersion(
    uint32_t *tlApiVersion);

/** Get information about the underlying MobiCore version.
 * The function tlApiGetMobicoreVersion() provides the MobiCore product id and
 * version information about the various MobiCore interfaces as defined in mcVersionInfo.h
 *
 * @param mcVersionInfo     pointer to version information structure.
 * @returns TLAPI_OK if version has been set
 * @returns E_TLAPI_NULL_POINTER    if one parameter is a null pointer.
 */
_TLAPI_EXTERN_C tlApiResult_t tlApiGetMobicoreVersion(
    mcVersionInfo_t *mcVersionInfo);


/** Get information about the hardware platform.
 * The function tlApiGetPlatformInformation() provides information about the current hardware platform.
 *
 * @param platformInfo pointer to tlApiPlatformInfo_t structure that receives the platform information.
 *
 * @return TLAPI_OK if character c has successfully been read.
 * @return E_TLAPI_BUFFER_TOO_SMALL if mcPlatformInfo.size is too small. On return mcPlatformInfo.size will be set to the required length.
 */
_TLAPI_EXTERN_C tlApiResult_t tlApiGetPlatformInfo(
    tlApiPlatformInfo_t    *platformInfo);

/** Terminate the Trustlet with a exit code.
 * Trustlets can use the tlApiExit() to terminate themselves and return an exit code. This
 * can be used if the initialization fails or an unrecoverable error occurred. The Trustlet
 * will be terminated immediately and this function will not return.
 *
 * @param exitCode exit code
 *
 * @return there is no return code, since the function will not return
 */
_TLAPI_EXTERN_C _TLAPI_NORETURN void tlApiExit(
    uint32_t    exitCode);

/** Get the System on Chip Universal Identifier.
 * @param suid pointer to Suid structure that receives the Suid data
 * @returns TLAPI_OK                if Suid has been successfully read.
 * @returns E_TLAPI_NULL_POINTER    if one parameter is a null pointer.
 */
_TLAPI_EXTERN_C tlApiResult_t tlApiGetSuid(
    mcSuid_t            *suid);

/** Check whether the device binding procedure has been executed already.
 * @param   isBound                 true if device is bound; false otherwise.
 * @returns TLAPI_OK                if operation was successful.
 * @returns E_TLAPI_NULL_POINTER    if one parameter is a null pointer.
 */
_TLAPI_EXTERN_C tlApiResult_t tlApiIsDeviceBound(
    bool  *isBound);

/** Privileged function that persistently sets the device into "device binding has been executed" state.
 * @note    Only system trustlets may call this function.
 * @returns TLAPI_OK                if operation was successful.
 * @returns E_TLAPI_SEC_DEVICE_ALREADY_BOUND    is device is already bound
 * @returns E_TLAPI_SEC_NOT_ALLOWED if not called by a System Trustlet
 */
_TLAPI_EXTERN_C tlApiResult_t tlApiBindDevice(
    void);

#endif // __TLAPIMCSYSTEM_H__

/** @} */
