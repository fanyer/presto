/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/** @file prefsfile-capabilities.h
 *
 * Defines the capabilites available in this version of the prefsfile module.
 */

#ifndef PREFSILFE_CAPABILITIES_H
#define PREFSILFE_CAPABILITIES_H

/** The PrefsFile::ClearSectionL() API is available.
  * Added 2005-08 on Gigo 1. */
#define PF_CAP_CLEARSECTION

/** PrefsFile can write to the global file.
  * Added 2005-12 on Gigo 1. */
#define PF_CAP_WRITE_GLOBAL

/** The IsSectionLocal() and IsKeyLocal() APIs are available for
  * PrefsFile.
  * Added 2006-01 on Gigo 1. */
#define PF_CAP_ISKEYLOCAL

/** The IniAccessor::IsRecognizedHeader() API is available.
  * Added 2006-01 on Gigo 1. */
#define PF_CAP_ISRECOGNIZEDHEADER

/** IniAccessor have been easier to override.
  * Added 2007-07 on Gigo 1. */
#define PF_CAP_INI_CHANGED_PRIVATE_METHODS

#endif
