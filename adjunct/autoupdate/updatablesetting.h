/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

/** \file
 * This file declares the UpdatableResource base class representing resources
 * that can be kept updated. The two subclasses UpdatableFile and 
 * UpdatableSetting are also defined here.
 *
 * @author Marius Blomli mariusab@opera.com
 */

#ifndef _UPDATEABLESETTING_H_INCLUDED_
#define _UPDATEABLESETTING_H_INCLUDED_

#ifdef AUTO_UPDATE_SUPPORT

#include "adjunct/autoupdate/updatableresource.h"

/**
 * This class is a representation of a preference setting that is to be 
 * kept up to date by the auto update system.
 * @see UpdatableResource::GetHashKey
 */
class UpdatableSetting: public UpdatableResource
{
public:
	UpdatableSetting();

	/**
	 * Implementing UpdatableResource API.
	 */
	virtual UpdatableResourceType	GetType() { return RTSetting; }
	virtual ResourceClass			GetResourceClass() { return Setting; }
	virtual OpFileLength			GetSize() const { return 0; }
	virtual OP_STATUS				UpdateResource();
	virtual BOOL					CheckResource() { return TRUE; }
	virtual OP_STATUS				Cleanup() { return OpStatus::OK; }
	virtual const uni_char*			GetResourceName() { return UNI_L("Setting"); }
	virtual BOOL					UpdateRequiresUnpacking() { return FALSE; }
	virtual BOOL					UpdateRequiresRestart() { return FALSE; }
	virtual BOOL					VerifyAttributes();

	BOOL							IsUpdateCheckInterval();
};

#endif // AUTO_UPDATE_SUPPORT

#endif // _UPDATEABLESETTING_H_INCLUDED_
