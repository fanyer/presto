// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Adam Minchinton
//
#include "core/pch.h"

#include "adjunct/quick/managers/DesktopSecurityManager.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

DesktopSecurityManager::DesktopSecurityManager() :
	m_inited(FALSE)
{

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

DesktopSecurityManager::~DesktopSecurityManager()
{
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS DesktopSecurityManager::Init()
{
	m_inited = TRUE;

	return OpStatus::OK;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

BOOL DesktopSecurityManager::IsHighSecurity(OpDocumentListener::SecurityMode mode)
{
	// Check for high security or above
	if (mode == OpDocumentListener::HIGH_SECURITY || IsEV(mode))
		return TRUE;
	
	return FALSE;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

BOOL DesktopSecurityManager::IsEV(OpDocumentListener::SecurityMode mode)
{
	// Add all of the EV modes in the switch
	switch (mode)
	{
		case OpDocumentListener::EXTENDED_SECURITY:
#ifdef WIC_CAP_SOME_EXTENDED_SECURITY
		case OpDocumentListener::SOME_EXTENDED_SECURITY:
#endif // WIC_CAP_SOME_EXTENDED_SECURITY
		{
			return TRUE;
		}
	}
	
	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

