/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef SECMAN_USERCONSENT

#include "modules/security_manager/src/security_persistence.h"

ChoicePersistenceType ToChoicePersistence(OpPermissionListener::PermissionCallback::PersistenceType persistence)
{
	switch(persistence)
	{
	case OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_NONE:    return PERSISTENCE_NONE;
	case OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_RUNTIME: return PERSISTENCE_RUNTIME;
	case OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_SESSION: return PERSISTENCE_SESSION;
	case OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_ALWAYS:  return PERSISTENCE_FULL;
	}
	OP_ASSERT(!"Unknown persistence value");
	return PERSISTENCE_NONE;
}

OpPermissionListener::PermissionCallback::PersistenceType ToPermissionListenerPersistence(ChoicePersistenceType persistence)
{
	switch (persistence)
	{
	case PERSISTENCE_RUNTIME:   return OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_RUNTIME;
	case PERSISTENCE_SESSION:   return OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_SESSION;
	case PERSISTENCE_FULL:      return OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_ALWAYS;
	default:
		OP_ASSERT(!"Unknown persistence value");
	case PERSISTENCE_NONE:      return OpPermissionListener::PermissionCallback::PERSISTENCE_TYPE_NONE;
	}
}

BOOL3 ToBool3(UserConsentType consent)
{
	return static_cast<BOOL3>(consent);
}

UserConsentType ToUserConsentType(BOOL3 is_allowed)
{
	return static_cast<UserConsentType>(is_allowed);
}

#endif // SECMAN_USERCONSENT
