/** @addtogroup CMP
 * @{
 *
 * @file
 * TlCm error return code definitions.
 * Definition of all possible TlCm rror return codes.
 *
 * <!-- Copyright Giesecke & Devrient GmbH 2009-2012 -->
 */

#ifndef TLCMERROR_H_
#define TLCMERROR_H_

#define  SUCCESSFUL                                  0x00000000

#define RET_ERR_EXT_UNKNOWN_COMMAND                  0xE0000000
#define RET_ERR_EXT_SECURITY_STATUS_NOT_SATISFIED    0xE0000010
#define RET_ERR_EXT_SECURE_MESSAGING_FAILED          0xE0000020
#define RET_ERR_EXT_INCORRECT_PARAMETERS             0xE0000030
#define RET_ERR_EXT_REFERENCED_DATA_INVALID          0xE0000040
#define RET_ERR_EXT_REFERENCED_DATA_NOT_FOUND        0xE0000050
#define RET_ERR_EXT_METHOD_BLOCKED                   0xE0000060
#define RET_ERR_EXT_CONDITIONS_OF_USE_NOT_SATISFIED  0xE0000070
#define RET_ERR_EXT_DEVICE_ALREADY_BOUND             0xE0000080
#define RET_ERR_EXT_ALREADY_REGISTERED               0xE0000090
#define RET_ERR_EXT_ALREADY_ACTIVATED                0xE00000A0
#define RET_ERR_EXT_NOT_REGISTERED                   0xE00000B0
#define RET_ERR_EXT_NOT_ACTIVATED                    0xE00000C0
#define RET_ERR_EXT_CONTAINER_FULL                   0xE00000D0
#define RET_ERR_EXT_INTERNAL_ERROR                   0xE0001000

#define RET_ERR_EXT_UNSPECIFIED                      0xEEEEEEEE

#endif // TLCMERROR_H_

/** @} */
