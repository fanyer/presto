/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef SECURITY_PERSISTENCE_H
#define SECURITY_PERSISTENCE_H

#include "modules/windowcommander/OpWindowCommander.h"

/// How long to remember user's choice.
enum ChoicePersistenceType {
	PERSISTENCE_NONE	= 0,
	PERSISTENCE_RUNTIME = 1,	//< Save choice for the current document
	PERSISTENCE_SESSION	= 2,	//< Save choice for session duration
	PERSISTENCE_FULL	= 3		//< Save choice for ever
};

#ifdef SECMAN_USERCONSENT

enum UserConsentType {
	ALLOWED = YES,
	DENIED  = NO,
	ASK     = MAYBE
};

/** Responsible for persisting user's choice regarding security rules.
 */
class OpSecurityPersistenceProvider
{
public:
	virtual ~OpSecurityPersistenceProvider() { }

	/** Checks whether the operation is allowed.

	    \param persistence Set to the persistence of the stored choice.
	                       Only valid if returned value is different than ASK.
	                       May be NULL.
	    \return ALLOWED/DISALLOWED in case information is available,
	            ASK if the user should be asked for confirmation. */
	virtual UserConsentType IsAllowed(ChoicePersistenceType* persistence = NULL) const = 0;

	/** Sets user's choice in persistent storage.

	    \param persistence_type type of persistence. For PERSISTENCE_NONE does nothing.
	    \param is_allowed whether the operation is allowed or not (i.e. the value to save). */
	virtual OP_STATUS SetIsAllowed(BOOL is_allowed, ChoicePersistenceType persistence) = 0;
};

ChoicePersistenceType ToChoicePersistence(OpPermissionListener::PermissionCallback::PersistenceType persistence);
OpPermissionListener::PermissionCallback::PersistenceType ToPermissionListenerPersistence(ChoicePersistenceType persistence);

UserConsentType ToUserConsentType(BOOL3 is_allowed);

#endif // SECMAN_USERCONSENT

#endif // SECURITY_PERSISTENCE_H
