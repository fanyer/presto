/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/** \file
 * This implementation of the PlatformUpdater interface is a cheat to get 
 * things rolling.
 *
 * @todo Make a real implementation of WindowsUpdater in platform code, and 
 * remove the stub in platformupdater.cpp.
 *
 * @author Marius Blomli mariusab@opera.com
 */

#include "core/pch.h"

#ifdef AUTO_UPDATE_SUPPORT

#include "adjunct/autoupdate/platformupdater.h"

OP_STATUS PlatformUpdater::Create(PlatformUpdater** new_object)
{
	WindowsUpdater* wu = OP_NEW(WindowsUpdater, ());
	if (!wu)
		return OpStatus::ERR_NO_MEMORY;
	*new_object = wu;
	return OpStatus::OK;
}

WindowsUpdater::WindowsUpdater()
{
	// Stub...
}

WindowsUpdater::~WindowsUpdater()
{
	// Stub...
}

BOOL WindowsUpdater::CheckUpdate()
{
	// Stub...
	return FALSE;
}

OP_STATUS WindowsUpdater::InitiateUpdate()
{
	// Stub...
	return OpStatus::ERR;
}

BOOL WindowsUpdater::HasInstallPrivileges()
{
	// Stub...
	return FALSE;
}

////////////////////////////////////////////////////////////////////////////////////////////////

#endif // AUTO_UPDATE_SUPPORT
