/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/widgetruntime/pi/PlatformGadgetInstallerDialog.h"
#include "adjunct/widgetruntime/GenericGadgetInstallerDialog.h"

#define BOOL NSBOOL
#import <AppKit/AppKit.h>
#undef BOOL

OP_STATUS PlatformGadgetInstallerDialog::Create(GadgetInstaller installer,
		PlatformGadgetInstallerDialog** dialog)
{
	OP_STATUS status = OpStatus::ERR;
	
	OP_ASSERT(NULL != dialog);

	if (NULL != dialog)
	{
		*dialog = GenericGadgetInstallerDialog::CreateInstallerDialog(
				installer);
		status = NULL != *dialog ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
	}
	
	return status;
}

#endif // WIDGET_RUNTIME_SUPPORT
