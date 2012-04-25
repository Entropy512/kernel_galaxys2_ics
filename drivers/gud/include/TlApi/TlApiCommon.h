/**
 * Common function for all TlApi Header files.
 * @file
 * <!-- Copyright Giesecke & Devrient GmbH 2009-2012 -->
 */
#ifndef __TLAPICOMMON_H__
#define __TLAPICOMMON_H__

//=============================================================================
// Compiler settings
//=============================================================================

// _TLAPI_EXTERN_C tells the C++ compiler, that this function is a plain C
// function. It has no effect when using a plain C compiler. Usually, this is
// defined to 'extern "C"' in most compilers. define it to nothing if you
// compiler does no support it.
#if !defined(_TLAPI_EXTERN_C)
    #error "_TLAPI_EXTERN_C is not defined."
#endif


// _TLAPI_NORETURN tells the compiler, that this function will not return. This
// allows further optimization of the code. For GCC and ARMCC this defines to
// '__attribute__((noreturn))'. Define it to nothing if your compiler does not
// support it.
#if !defined(_TLAPI_NORETURN)
    #error "_TLAPI_NORETURN is not defined."
#endif


//=============================================================================
// TlApi constants
//=============================================================================

#define TLAPI_INFINITE_TIMEOUT  ((uint32_t)(-1))    /**< Wait infinite for a response of the MC. */
#define TLAPI_NO_TIMEOUT        0                   /**< Do not wait for a response of the MC. */


#endif // __TLAPICOMMON_H__
