/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Michal Zajaczkowski
*/

#ifndef _ST_ADDITIONCHECKER_H_INCLUDED_
#define _ST_ADDITIONCHECKER_H_INCLUDED_

#ifdef AUTO_UPDATE_SUPPORT
#ifdef SELFTEST

#include "adjunct/autoupdate/additionchecker.h"
#include "adjunct/autoupdate/additioncheckerlistener.h"

class ST_AdditionChecker:
	public AdditionChecker,
	public AdditionCheckerListener
{
public:
	ST_AdditionChecker();

	OP_STATUS Init();
	OP_STATUS CheckForAddition(UpdatableResource::UpdatableResourceType type, const char* key, BOOL is_resource_expected);

	void OnAdditionResolved(UpdatableResource::UpdatableResourceType type, const OpStringC& key, UpdatableResource* resource);
	void OnAdditionResolveFailed(UpdatableResource::UpdatableResourceType type, const OpStringC& key);

private:
	UpdatableResource::UpdatableResourceType m_expected_type;
	OpString m_expected_key;
	BOOL m_is_resource_expected;
};

#endif
#endif
#endif
