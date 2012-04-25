/** @addtogroup DRAPI_COM
 *
 * This is a wrapper to include standard header files in device drivers.
 *
 * <!-- Copyright Giesecke & Devrient GmbH 2009-2012 -->
 *
 */

#ifndef __DRAPICOMMON_H__
#define __DRAPICOMMON_H__

typedef uint32_t        stackEntry_t;
typedef stackEntry_t    *stackEntry_ptr;
typedef stackEntry_ptr  stackTop_ptr;
typedef stackEntry_ptr  stackBottom_ptr;

// IMPROVEMENT-2012-01-18 (hollyr, galkag) heidera This is also defined in mctype.h, which can cause conflicts. We
//       should have this in one header file only
#define SHIFT_1KB               (10U) /**<  SIZE_1KB is 1 << SHIFT_1KB aka. 2^SHIFT_1KB. */
#define SIZE_1KB                (1 << SHIFT_1KB) /**< Size of 1 KiB. */

#define SHIFT_4KB               (12U) /**<  SIZE_4KB is 1 << SHIFT_4KB aka. 2^SHIFT_4KB. */
#define SIZE_4KB                (1 << SHIFT_4KB) /**< Size of 1 KiB. */
typedef uint8_t                 page4KB_t[SIZE_4KB]; /**< 4 KiB page. */
typedef page4KB_t               *page4KB_ptr; /**< pointer to 4 KiB page. */


#define PTR2VAL(p)      ((uintptr_t)(p))
#define VAL2PTR(v)      ((addr_t)(v))
// lint complains about casting a function pointer to addr_t directly. So we
// use this. A compiler will eliminate this anyway.
#define FUNC_PTR(func)  VAL2PTR( PTR2VAL( func ) )

//==============================================================================

typedef unsigned int	u32_t;
typedef unsigned short	u16_t;
typedef unsigned char	u08_t;
typedef u32_t			word_t;


typedef word_t drApiResult_t;

typedef word_t  taskid_t,   *taskid_ptr;     /**< task id data type. */
typedef word_t  threadno_t, *threadno_ptr;   /**< thread no. data type. */
typedef word_t  threadid_t, *threadid_ptr;   /**< thread id data type. */

typedef word_t  intrNo_t, *intrNo_ptr;      /**< interrupt number. */

#endif //__DRAPICOMMON_H__
