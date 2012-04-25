/** @addtogroup TLAPI_SEC
 * @{
 *
 * The secure object API provides integrity, confidentiality and authenticity
 * for data that is sensitive but needs to be stored in untrusted (normal world)
 * memory. Secure objects provide device binding. Respective objects are only
 * valid on a specific device.
 *
 * There are two core operations of this API:
 *
 * - wrap() which encloses user data in a secure object.
 * - unwrap() which extracts user data from a secure object.
 *
 * A secure object looks like this:
 *
 *  <pre>
 *  <code>
 *
 *  +--------+------------------+------------------+--------+---------+
 *  | Header |   plain-data     |  encrypted-data  |  hash  | padding |
 *  +--------+------------------+------------------+--------+---------+
 *
 *           /---------------user data ------------/
 *
 *  /--------/---- plainLen ----/-- encryptedLen --/-- 32 --/- 1..16 -/
 *
 *  /-------------- checksum protected ------------/
 *
 *                              /-------------- encrypted ------------/
 *
 *  </code>
 *  </pre>
 *
 * DATA INTEGRITY:
 *
 * A secure object contains a message digest (hash, checksum) that ensures data
 * integrity of the user data. The hash value is computed and stored during the
 * wrap() operation (before data encryption takes place) and recomputed and
 * compared during the unwrap() operation (after the data has been decrypted).
 *
 * CONFIDENTIALITY:
 *
 * Secure objects are encrypted using context-specific keys that are never
 * exposed, neither to the normal world, nor to the Trustlet. It is up to the
 * user to define how many bytes of the user data are to be kept in plain text
 * and how many bytes are to be encrypted. The plain text part of a secure
 * object always precedes the encrypted part.
 *
 * AUTHENTICITY:
 *
 * As a means of ensuring the trusted origin of secure objects, the wrap
 * operation stores the Trustlet id (SPID, UUID) of the calling Trustlet in
 * the secure object header (as producer). This allows Trustlets to only accept
 * secure objects from certain partners. This is most important for scenarios
 * involving secure object sharing.
 *
 * CONTEXT:
 *
 * The concept of context allows for sharing of secure objects. At present there
 * are three kinds of context:
 *
 * - MC_SO_CONTEXT_TLT: Trustlet context. The secure object is confined to a
 *   particular Trustlet. This is the standard use case.
 *         - PRIVATE WRAPPING: If no consumer was specified, only the Trustlet
 *           that wrapped the secure object can unwrap it.
 *         - DELEGATED WRAPPING: If a consumer Trustlet is specified, only the
 *           Trustlet specified as 'consumer' during the wrap operation can
 *           unwrap the secure object.
 *           Note that there is no delegated wrapping with any other contexts.
 *
 * - MC_SO_CONTEXT_SP: Service provider context. Only Trustlets that belong to
 *   the same service provider can unwrap a secure object that was wrapped in
 *   the context of a certain service provider.
 *
 * - MC_SO_CONTEXT_DEVICE: Device context. All Trustlets can unwrap secure
 *   objects wrapped for this context.
 *
 * LIFETIME:
 *
 * The concept of a lifetime allows to limit how long a secure object is valid.
 * After the end of the lifetime, it is impossible to unwrap the object. At
 * present, three lifetimes are defined:
 *
 * - MC_SO_LIFETIME_PERMANENT: Secure Object does not expire.
 *
 * - MC_SO_LIFETIME_POWERCYCLE: Secure Object expires on reboot.
 *
 * - MC_SO_LIFETIME_SESSION: Secure Object expires when Trustlet session is closed.
 *   The secure object is thus confined to a particular session of a particular
 *   Trustlet.
 *   Note that session lifetime is only allowed for private wrapping in the Trustlet
 *   context MC_SO_CONTEXT_TLT.
 *
 * <!-- Copyright Giesecke & Devrient GmbH 2009-2012 -->
 */

#ifndef __TLAPISECURITY_H__
#define __TLAPISECURITY_H__

#include "mcSo.h"

#include "TlApi/TlApiCommon.h"
#include "TlApi/TlApiError.h"

/** Wraps user data given in source buffer and creates a secure object in the
 * destination buffer.
 *
 * The required size of the destination buffer (total size of secure object) can
 * be obtained through the MC_SO_SIZE() macro.
 *
 * Source and destination addresses may overlap, thus the following code is a
 * typical usage pattern:
 *
 *  <pre>
 *
 *  union {
 *      uint8_t src[100];
 *      uint8_t dst[MC_SO_SIZE(30, 70)]; // 30 bytes plain, 70 bytes encrypted.
 *  } buffer;
 *
 *  size_t soLen = sizeof(buffer);
 *
 *  // Fill source.
 *  buffer.src[0] = 'H';
 *  ...
 *
 *  if (TLAPI_OK == tlApiWrapObject(
 *      buffer.src, 30, 70, buffer.dst, &soLen, MC_SO_CONTEXT_TLT, NULL)) {
 *      ...
 *  }
 *
 * </pre>
 *
 * @param src User data.
 * @param plainLen Length of plain text user data (from beginning of src).
 * @param encryptedLen Length of to-be-encrypted user data (after plain text
 * user data).
 * @param dest Destination buffer (secure object).
 * @param [in, out] destLen [in] Length of the destination buffer. [out] Length
 * of secure object.
 * @param context Key context.
 * @param lifetime Expiry type of secure object.
 * @param consumer NULL or Trustlet/service provider identifier for delegated wrapping.
 * @return TLAPI_OK if operation was successful.
 * @retval E_TLAPI_NULL_POINTER If a pointer input parameter is NULL.
 * @retval E_TLAPI_INVALID_INPUT If an input parameter is invalid, for instance
 * if the maximum payload size is exceeded.
 * @retval E_TLAPI_CR_WRONG_OUPUT_SIZE If the output buffer is too small.
 */
_TLAPI_EXTERN_C tlApiResult_t tlApiWrapObject(
    const void *src,
    size_t plainLen,
    size_t encryptedLen,
    void *dest,
    size_t *destLen,
    mcSoContext_t context,
    mcSoLifeTime_t lifetime,
    const tlApiSpTrustletId_t *consumer);


/** Unwraps a secure object.
 *
 * Decrypts and verifies checksum of given object for the context indicated in
 * the secure object's header.
 *
 * Verifies and decrypts a secure object and stores the plain text to a given
 * location. For further details refer to tlApiWrapObject().
 *
 * After this operation, the source address contains the decrypted secure
 * object (whose user data starts immediately after the secure object header),
 * or the attempt of the decryption, which might be garbage, in case the
 * decryption failed (due to a wrong context, for instance).
 *
 * If dest is not NULL, copies the decrypted user data part to the specified
 * location, which may overlap with the memory occupied by the original secure
 * object.
 *
 * @param [in,out] src [in] Encrypted secure object, [out] decrypted secure
 * object.
 * @param [out] dest Address of user data or NULL if no extraction of user data
 * is desired.
 * @param [in,out] destLen [in] Length of destination buffer. [out] Length of
 * user data.
 *
 * @return TLAPI_OK if operation was successful.
 * @retval E_TLAPI_INVALID_INPUT If an input parameter is invalid.
 * @retval E_TLAPI_CR_WRONG_OUPUT_SIZE If the output buffer is too small.
 * @retval E_TLAPI_SO_WRONG_VERSION If the version of the secure object is not
 * supported.
 * @retval E_TLAPI_SO_WRONG_TYPE If the kind of secure object is not supported.
 * @retval E_TLAPI_SO_WRONG_LIFETIME If the kind of lifetime of the secure
 * object is not supported.
 * @retval E_TLAPI_SO_WRONG_CONTEXT If the kind of context of the secure object
 * is not supported.
 * @retval E_TLAPI_SO_WRONG_PADDING  If (after decryption) the padding of the
 * unencrypted data is wrong. This is usually an indication that the client
 * calling unwrap is not allowed to unwrap the secure object.
 * @retval E_TLAPI_SO_WRONG_CHECKSUM If (after decryption) the checksum over the
 * whole secure object (header and payload) is wrong. This is usually an
 * indication that the secure object has been tampered with.
 */
_TLAPI_EXTERN_C tlApiResult_t tlApiUnwrapObject(
    void *src,
    void *dest,
    size_t *destLen);

/** Real time sources in MobiCore */
typedef enum {
        TS_SOURCE_ILLEGAL       = 0,    /**< Illegal counter source value. */
        TS_SOURCE_SOFTCNT       = 1,    /**< monotonic counter that is reset upon power cycle. */
        TS_SOURCE_SECURE_RTC    = 2,    /**< Secure real time clock that uses underlying hardware clock. */
} tsSource_t;

/** Time stamp value*/
typedef uint64_t timestamp_t;

/** Retrieve a time stamp value.
 *
 * Returns a time stamp value. The time stamp value is granted to be monotonic depending on the
 * time stamp source it is based on. The implementation takes care that no time stamp overflow occurs.
 *
 * Two subsequent calls to the function return always two different time stamp values where the
 * second value is always bigger than the first one. If the first call to the function returns
 * val1 as time stamp value, second call returns val2. For the returned values the following rules apply:
 *
 * val2 >= (val1 + 1)
 *
 * When a time stamps source is not supported by the platform. The function returns E_TLAPI_NOT_IMPLEMENTED.
 *
 * @param ts [out] After successful execution the time stamp is returned in the reference variable.
 * @param source [in] Time stamp source
 *
 * @returns TLAPI_OK if operation was successful.
 * @returns E_TLAPI_NOT_IMPLEMENTED if source is not supported by the platform.
 */
_TLAPI_EXTERN_C tlApiResult_t tlApiGetTimeStamp(
    timestamp_t *ts,
    tsSource_t  source
);

#endif // __TLAPISECURITY_H__

/** @} */
