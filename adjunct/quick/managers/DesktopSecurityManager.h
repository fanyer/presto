// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Adam Minchinton
//

#ifndef __DESKTOPSECURITY_MANAGER_H__
#define __DESKTOPSECURITY_MANAGER_H__

#include "adjunct/quick/managers/DesktopManager.h"
#include "modules/windowcommander/OpWindowCommander.h"

#define g_desktop_security_manager (DesktopSecurityManager::GetInstance())

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

class DesktopSecurityManager : public DesktopManager<DesktopSecurityManager>
{
public:
	DesktopSecurityManager();
	~DesktopSecurityManager();

	// Initialise all the things the Opera Account Manager needs
	OP_STATUS	Init();
	BOOL		IsInited() { return m_inited; }

	// Combines all the possible EV modes to make coding easier
	BOOL		IsEV(OpDocumentListener::SecurityMode mode);

	// Combines all the possible high security modes to make coding easier. (i.e. High, SomeEV, EV)
	BOOL		IsHighSecurity(OpDocumentListener::SecurityMode mode);
private:
	BOOL m_inited;				// Set to TRUE after a successful Init() call
};

#endif // __DESKTOPSECURITY_MANAGER_H__
