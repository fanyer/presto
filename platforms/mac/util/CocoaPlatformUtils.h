/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef _COCOA_PLATFORM_UTILS_H_
#define _COCOA_PLATFORM_UTILS_H_

namespace CocoaPlatformUtils
{
	BOOL IsGadgetInstallerStartup();

	OP_STATUS BounceDockIcon();
	OP_STATUS HasAdminAccess(BOOL &has_admin_access);
}

#endif //_COCOA_PLATFORM_UTILS_H_
