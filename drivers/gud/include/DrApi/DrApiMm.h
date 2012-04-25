/** @addtogroup DRAPI_COM
 * @{
 *
 * Device Driver Kit Api Memory Management definitions
 *
 * API definitions for memory management fuctions in Mobicore Device Drivers.
 *
 * @file
 * Device Driver Kit functions.
 *
 * <!-- Copyright Giesecke & Devrient GmbH 2009-2012 -->
 */

#ifndef __DRAPIMM_H__
#define __DRAPIMM_H__

#include "DrApiCommon.h"

//------------------------------------------------------------------------------
/** Definitions */

/** Memory mapping attributes. */
// IMPROVEMENT-2012-01-18-heidera (hollyr, galkag) Remove duplicated codes
#define MAP_READABLE            (1U << 0)               /**< mapping does/shall have the ability to do read access. */
#define MAP_WRITABLE            (1U << 1)               /**< mapping does/shall have the ability to do write access. */
#define MAP_EXECUTABLE          (1U << 2)               /**< mapping does/shall have the ability to do program execution. */
#define MAP_UNCACHED            (1U << 3)               /**< mapping does/shall have uncached memory access. */
#define MAP_IO                  (1U << 4)               /**< mapping does/shall have memory mapped I/O access. Will ignore MAP_UNCACHED, as this would be implied anyway. */

//------------------------------------------------------------------------------
/** Maximum number of parameter . */
#define MAX_MAR_LIST_LENGTH 8                      /**< Maximum list of possible marshaling parameters. */
/** Marshaled union. */
typedef struct {
    uint32_t     functionId;                       /**< Function identifier. */
    union {
        uint32_t                            parameter[MAX_MAR_LIST_LENGTH];   /* untyped parameter list (expands union to 8 entries) */
    } payload;
} marshalingParam_t, *marshalingParam_ptr;


//------------------------------------------------------------------------------
/** Address translation from trustlet to driver address space.
 * Translate an address/pointer given by a trustlet to the driver's mapping.
 * Checks for correct address range and null pointer.
 *
 * @param addr Address in trustlet address space.
 * @return address in driver virtual space, or NULL in case of any error.
 */
addr_t drApiAddrTranslateAndCheck(addr_t addr);

//------------------------------------------------------------------------------
/** map requesting client and return translated pointer to request parameters
 *
 * @param ipcReqClient client requesting a service
 * @param params pointer to marshaled parameter in client address space
 * @return pointer to parameter for request in the current address space
 */
marshalingParam_ptr drApiMapClientAndParams(
    threadid_t  ipcReqClient,
    uint32_t    params
);

//------------------------------------------------------------------------------

/** Map a physical page to a virtual address.
 *  All addresses and lengths must be multiples of page size (4K).
 *
 * @param startVirt virtual address in task's address space
 * @param len Length of area
 * @param startPhys physical address of hardware
 * @param attr mapping attributes
 */

drApiResult_t drApiMapPhys(
    const addr_t      startVirt,
    const uint32_t    len,
    const addr_t      startPhys,
    const uint32_t    attr
);

/** Remove mapping for a virtual pages.
 *  All addresses and lengths must be multiples of page size (4K).
 *
 * @param startVirt virtual address in task's address space
 * @param len Length of area
 */

drApiResult_t drApiUnmap(
    const addr_t    startVirt,
    const uint32_t  len
);


drApiResult_t drApiForwardMapping(
    const taskid_t  taskId,
    const addr_t    startVirtDst,
    const uint32_t  len,
    const addr_t    startVirtSrc,
    const uint32_t  attr
);

drApiResult_t drApiMapClientToServer(
    const taskid_t     serverTaskId,
    const taskid_t     clientTaskId
);


//------------------------------------------------------------------------------
/** Unmap a single page.
 *
 * @param startVirt virtual address in task's address space
 *
 */
void drApiUnmapPage4KB(
    const page4KB_ptr   virtPage
);

//------------------------------------------------------------------------------
/** Map a single physical page to a virtual address
 *
 * @param physPage virtual address in task's address space
 * @param startPhys physical address of hardware
 * @param attr mapping attributes
 */
void drApiMapPhysPage4KB(
    const page4KB_ptr   virtPage,
    const page4KB_ptr   physPage,
    const uint32_t      attr
);


//------------------------------------------------------------------------------
/** Map physical page with hardware interface
 *
 * @param physPage virtual address in task's address space
 * @param startPhys physical address of hardware
 */
void drApiMapPhysPage4KBWithHardware(
    const page4KB_ptr   virtPage,
    const page4KB_ptr   physPage
);


#endif // __DRAPIMM_H__
