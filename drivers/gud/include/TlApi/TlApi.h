/*
 *
 * <!-- Copyright Giesecke & Devrient GmbH 2009-2012 -->
 *
 */

#ifndef __TLAPI_H__
#define __TLAPI_H__

/**
 *
 * @defgroup FUNCTIONS  Trustlet API functions
 *
 *      @defgroup TLAPI_COM Inter-world communication functions [COM]
 *      @ingroup  FUNCTIONS
 *
 *      @defgroup TLAPI_CR  Crypto-driver interface functions [CR]
 *      @ingroup  FUNCTIONS
 *
 *      @defgroup TLAPI_SEC Security support functions [SEC]
 *      @ingroup  FUNCTIONS
 *
 *      @defgroup TLAPI_SYS MobiCore system functions [SYS]
 *      @ingroup  FUNCTIONS
 *
 *      @defgroup TLAPI_KPD Secure key pad access functions [KPD]
 *      @ingroup  FUNCTIONS
 *
 * @defgroup CONSTANTS  Trustlet API constant definitions
 *
 *      @defgroup TLAPI_ERR Error codes
 *      @ingroup  CONSTANTS
 *
 * ------------------------------------------------------------------------------
 *
 * @mainpage Trustlet API - TlApi
 *
 * Trustlet API
 *
 * <h2>Introduction</h2>
 * The Trustlet API is the interface between the MobiCore system and Trustlets.
 * For a detailed specification of the MobiCore communication mechanisms
 * please refer to the "MobiCore Low-level Interface Specification".
 *
 * <h2>System overview</h2>
 * The MC system is structured as follows.
 * @image html DoxyOverviewTlApi500x.png "MobiCore system overview"
 * @image latex DoxyOverviewTlApi500x.png "MobiCore system overview" width=12cm
 *
 * <h2>Version history</h2>
 * <table class="customtab">
 * <tr><td width="100px"><b>Date</b></td><td width="80px"><b>Version</b></td><td><b>Changes</b></td></tr>
 * <tr><td>2009-02-20</td><td>0.1</td><td>Initial Release</td></tr>
 * <tr><td>2009-03-02</td><td>0.2</td>
 *   <td> Further documentation after review.<br>
 *        Additions to  keypad interface <br>
 *        Refinement of crypto driver errorcodes and interface<br>
 *        Additions to driver interface.</td>
 * </tr>
 * <tr><td>2009-03-12</td><td>0.3</td>
 *   <td> Fixed documentation issue in TlApiError.h</td>
 * </tr>
 * <tr><td>2009-03-20</td><td>0.4</td>
 *   <td> Corrected minor issues after review.</td>
 * </tr>
 * <tr><td>2009-05-15</td><td>0.5</td>
 *   <td>Corrected minor issues.</td>
 * </tr>
 * <tr><td>2009-06-13</td><td>0.6</td>
 *   <td> Introduced modularization within MC documentation.</td>
 * </tr>
 * <tr><td>2010-05-04</td><td>0.7</td>
 *   <td> Minor fixes.</td>
 * </tr>
 * <tr><td>2010-06-09</td><td>0.8</td>
 *   <td> Review and restructuring of the documentation.</td>
 * </tr>
 * <tr><td>2011-07-19</td><td>0.9</td>
 *   <td>update to reflect current code changes.</td>
 * </tr>
 * <tr><td>2011-11-09</td><td>0.9.1</td>
 *   <td>Refactored tlApiGetMcVersion().<br>
 *       Introduced new security functions: <br>
 *       - tlApiGetSuid(),<br>
 *       - tlApiIsDeviceBound(),<br>
 *       - tlApiBindDevice(),<br>
 *       - tlApiGetTimeStamp().</td>
 * </tr>
 * <tr><td>2011-11-24</td><td>0.9.2</td>
 *   <td>Restructuring.<br>
 *       Removed [DRV].
 *       Moved functions.</td>
 * </tr>
 * <tr><td>2011-11-24</td><td>1.0</td>
 *   <td>Release version</td>
 * </tr>
 * </table>
 */

#include "TlApi/version.h"
#include "TlApi/TlApiCom.h"
#include "TlApi/TlApiCrypto.h"
#include "TlApi/TlApiKeypad.h"
#include "TlApi/TlApiMcSystem.h"
#include "TlApi/TlApiSecurity.h"

#endif // __TLAPI_H__

/** @} */
