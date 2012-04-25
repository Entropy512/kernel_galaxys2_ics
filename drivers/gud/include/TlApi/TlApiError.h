/** @addtogroup TLAPI_ERR
 * @{
 * TlApi return and error values.
 * Generic error codes valid for all TlApi functions are defined here.
 *
 * @file
 * TlApi return and error values.
 * Generic error codes valid for all TlApi functions are defined here.
 *
 * <!-- Copyright Giesecke & Devrient GmbH 2009-2012 -->
 */
#ifndef __TLAPIERROR_H__
#define __TLAPIERROR_H__

/** TlApi return values. */
typedef enum {
    TLAPI_OK                           = 0x00000000,    /**< Returns on successful execution of a function. */

    E_TLAPI_NOT_IMPLEMENTED            = 0x00000001,    /**< Function not yet implemented. */
    E_TLAPI_UNKNOWN                    = 0x00000002,    /**< Unknown error during TlApi usage. */
    E_TLAPI_UNKNOWN_FUNCTION           = 0x00000004,    /**< Function not known. */
    E_TLAPI_INVALID_INPUT              = 0x00000005,    /**< Input data is invalid. */
    E_TLAPI_INVALID_RANGE              = 0x00000006,    /**< If address/pointer. */
    E_TLAPI_BUFFER_TOO_SMALL           = 0x00000007,    /**< A buffer is too small. */
    E_TLAPI_INVALID_TIMEOUT            = 0x00000008,    /**< The chosen timeout value was invalid. */
    E_TLAPI_TIMEOUT                    = 0x00000009,    /**< Timeout expired. */
    E_TLAPI_NULL_POINTER               = 0x0000000a,    /**< Null pointer. */
    E_TLAPI_INVALID_PARAMETER          = 0x0000000b,    /**< Invalid parameter. */

    // Communication error codes
    E_TLAPI_COM_WAIT                   = 0x00010001,    /**< Waiting for a notification failed. */
    E_TLAPI_COM_ERROR                  = 0x00010002,    /**< Internal communication error. */

    // Crypto error codes
    E_TLAPI_CR_HANDLE_INVALID          = 0x00020001,    /**< No running session is associated with this handle value or caller has
                                                            not permission to access the session associated with this handle value. */
    E_TLAPI_CR_ALGORITHM_NOT_AVAILABLE = 0x00020002,    /**< Algorithm is not available for the caller. */
    E_TLAPI_CR_ALGORITHM_NOT_SUPPORTED = 0x00020003,    /**< The intended algorithm usage is not supported. */
    E_TLAPI_CR_WRONG_INPUT_SIZE        = 0x00020004,    /**< Input data (message or data to be encrypted or decrypted) is too short or too long. */
    E_TLAPI_CR_WRONG_OUPUT_SIZE        = 0x00020005,    /**< Provided Output buffer is of wrong size. */
    E_TLAPI_CR_INCONSISTENT_DATA       = 0x00020006,    /**< Inconsistency of data was determined. */
    E_TLAPI_CR_NO_FUNCTIONALITY        = 0x00020007,    /**< The called function is not supported. */
    E_TLAPI_CR_OUT_OF_RESOURCES        = 0x00020008,    /**< No more additional resources available. */
    E_TLAPI_CR_NO_SUCH_ALGORITHM       = 0x00020009,    /**< No more additional resources available. */

    // Error codes for keypad driver
    E_TLAPI_KPD                        = 0x00040001,    /**< Unspecified keypad error. */
    E_TLAPI_KPD_NO_SUCH_DEVICE         = 0x00040002,    /**< No such keypad available. */
    E_TLAPI_KPD_GRABBED                = 0x00040003,    /**< The keypad is currently grabbed by another Trustlet. */
    E_TLAPI_KPD_FORCE_UNGRABBED        = 0x00040004,    /**< The keypad is forced ungrabbed. */
    E_TLAPI_KPD_UNGRAB                 = 0x00040005,    /**< The keypad could not be ungrabbed. */

    // Error codes for drivers
    E_TLAPI_DRV_UNKNOWN                = 0x00050001,    /**< Unspecified driver error. */
    E_TLAPI_DRV_NO_SUCH_DRIVER         = 0x00050002,    /**< Unknown driver, bad driver ID. */
    E_TLAPI_DRV_DRIVER_IN_USE          = 0x00050003,    /**< Driver is already in use. */
    E_TLAPI_DRV_INVALID_PARAMETERS     = 0x00050004,    /**< Driver parameters invalid. */
    E_TLAPI_DRV_INVALID_FILE           = 0x00050005,    /**< File handle invalid. */
    E_TLAPI_DRV_NOT_SUPPORTED          = 0x00050006,    /**< Operation (read, write, seek) not supported. */
    E_TLAPI_DRV_INVALID_LENGTH         = 0x00050007,    /**< Invalid length. Operation failed. */
    E_TLAPI_DRV_INVALID_POSITION       = 0x00050008,    /**< Seek positioning failed. */
    E_TLAPI_DRV_NO_MORE_DRIVERS        = 0x00050009,    /**< No more drivers available (tlApiGetDrivers()). */

    // Secure object specific.
    E_TLAPI_SO_WRONG_CONTEXT           = 0x00060001,    /**< Illegal (unsupported) secure object context. */
    E_TLAPI_SO_WRONG_PADDING           = 0x00060002,    /**< Wrong padding of secure object. */
    E_TLAPI_SO_WRONG_CHECKSUM          = 0x00060003,    /**< Wrong secure object checksum. */
    E_TLAPI_SO_CONTEXT_KEY_FAILED      = 0x00060004,    /**< Derivation of context key failed. */
    E_TLAPI_SO_WRONG_LIFETIME          = 0x00060005,    /**< Illegal (unsupported) secure object lifetime. */
    E_TLAPI_SO_WRONG_VERSION           = 0x00060006,    /**< Illegal (unsupported) secure object version. */
    E_TLAPI_SO_WRONG_TYPE              = 0x00060007,    /**< Illegal (unsupported) secure object type. */

    // Device binding specific.
    E_TLAPI_SEC_DEVICE_ALREADY_BOUND   = 0x00070001,    /**< Device binding has already been performed. */
    E_TLAPI_SEC_NOT_ALLOWED            = 0x00070002,    /**< Operation is not permitted for Service Provider Trustlets. */
} tlApiResult_t;


#endif // __TLAPIERROR_H__

/** @} */
