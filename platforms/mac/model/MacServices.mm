/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/model/MacServices.h"
#include "platforms/mac/model/OperaNSWindow.h"

#if defined(SUPPORT_OSX_SERVICES) && defined(NO_CARBON)

////////////////////////////////////////////////////////////////////////

MacOpServices::MacOpServices()
{
}

////////////////////////////////////////////////////////////////////////

void MacOpServices::InstallServicesHandler()
{
	[OperaNSWindow InstallServicesHandler];
}

////////////////////////////////////////////////////////////////////////

void MacOpServices::Free()
{

}

////////////////////////////////////////////////////////////////////////

#endif // defined(SUPPORT_OSX_SERVICES) && defined(NO_CARBON)
