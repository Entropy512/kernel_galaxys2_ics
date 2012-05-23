/*
 * Linux 2.6.32 and later Kernel module for MODULE_PURPOSE
 *
 * Copyright (C) 2010-2012 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; see the file COPYING.  If not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#line 5
/* ****************************************************************************
 * Copyright (C) 2009 VMware, Inc. All rights reserved.
 * -- VMware Confidential
 * ***************************************************************************/

/**
 * @file
 *
 * @brief What version is this?
 *
 */

#ifndef _MVP_VERSION_H_
#define _MVP_VERSION_H_

#define INCLUDE_ALLOW_MVPD
#define INCLUDE_ALLOW_VMX
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_MONITOR
#define INCLUDE_ALLOW_GPL
#define INCLUDE_ALLOW_HOSTUSER
#include "include_check.h"
#include "utils.h"

/*
 * MVP Internal Version Numbering
 *
 * major    =>    Incremented by Marketing
 * minor    =>    Incremented feature release (requires legal signoff for release)
 * update   =>    Incremented for official external release
 * patch    =>    Incremented by engineering upon functional change/bug fix
 *
 * Note that the first 3 digits are present in VMwareReady.apk version string.
 *
 * Examples:
 *
 * 1.0.0.0    =>    First Official release to OEMs
 * 1.0.0.x    =>    Internal bug fixes to 1.0.0.0
 * 1.0.1.0    =>    External release of 1.0.0.x
 * 1.1.0.0    =>    External release of a 1.0.x.y that contains functional changes (i.e. guest network control)
 */
#define MVP_VERSION(major, minor, update, patch_level) \
   ((((major) & 0xFF) << 24) | \
    (((minor) & 0xFF) << 16) | \
    (((update) & 0xFF) << 8) | \
     ((patch_level) & 0xFF))

#define MVP_VERSION_CODE  MVP_VERSION(1,0,2,15)

#define MVP_VERSION_CODE_MAJORV(V_)  (((V_)>>24) & 0xFF)
#define MVP_VERSION_CODE_MINORV(V_)  (((V_)>>16) & 0xFF)
#define MVP_VERSION_CODE_UPDATEV(V_) (((V_)>> 8) & 0xFF)
#define MVP_VERSION_CODE_PATCHLV(V_) (((V_)    ) & 0xFF)

#define MVP_VERSION_CODE_MAJOR  MVP_VERSION_CODE_MAJORV(MVP_VERSION_CODE)
#define MVP_VERSION_CODE_MINOR  MVP_VERSION_CODE_MINORV(MVP_VERSION_CODE)
#define MVP_VERSION_CODE_UPDATE MVP_VERSION_CODE_UPDATEV(MVP_VERSION_CODE)
#define MVP_VERSION_CODE_PATCHL MVP_VERSION_CODE_PATCHLV(MVP_VERSION_CODE)


#define MVP_VERSION_CODE_FORMATSTR "V%d.%d.%d.%d"
#define MVP_VERSION_CODE_FORMATARGSV(V_)        \
   MVP_VERSION_CODE_MAJORV(V_),                 \
   MVP_VERSION_CODE_MINORV(V_),                 \
   MVP_VERSION_CODE_UPDATEV(V_),                \
   MVP_VERSION_CODE_PATCHLV(V_)
#define MVP_VERSION_CODE_FORMATARGS             \
   MVP_VERSION_CODE_FORMATARGSV(MVP_VERSION_CODE)


#define MVP_VERSION_FORMATSTR                        \
   MVP_VERSION_CODE_FORMATSTR                        \
   " compiled at %s based on revision %s by user %s."

#define MVP_VERSION_FORMATARGS      \
   MVP_VERSION_CODE_FORMATARGS,     \
   __DATE__,                        \
   MVP_STRINGIFY(master-1f6423d),     \
   MVP_STRINGIFY(rbenis)



#endif /* _MVP_VERSION_H_ */
