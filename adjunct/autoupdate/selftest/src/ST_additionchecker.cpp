/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Michal Zajaczkowski
 */

#include "core/pch.h"

#ifdef AUTO_UPDATE_SUPPORT
#ifdef SELFTEST

#include "modules/selftest/src/testutils.h"
#include "adjunct/autoupdate/selftest/src/ST_additionchecker.h"

ST_AdditionChecker::ST_AdditionChecker():
	m_expected_type(UpdatableResource::RTAll),
	m_is_resource_expected(FALSE)
{
}

OP_STATUS ST_AdditionChecker::Init()
{
	RETURN_IF_ERROR(AdditionChecker::Init());
	return AddListener(this);
}

OP_STATUS ST_AdditionChecker::CheckForAddition(UpdatableResource::UpdatableResourceType type, const char* key, BOOL is_resource_expected)
{
	m_expected_type = type;
	RETURN_IF_ERROR(m_expected_key.Set(key));
	m_is_resource_expected = is_resource_expected;

	return AdditionChecker::CheckForAddition(m_expected_type, m_expected_key);
}

void ST_AdditionChecker::OnAdditionResolved(UpdatableResource::UpdatableResourceType type, const OpStringC& key, UpdatableResource* resource)
{
	// It shouldn't hurt if we can't remore the listener since the listener is this object
	OpStatus::Ignore(RemoveListener(this));
	if (type == m_expected_type)
	{
		if (key.CompareI(m_expected_key) == 0)
		{
			if (m_is_resource_expected && resource)
				ST_passed();
			else if (!m_is_resource_expected && !resource)
				ST_passed();
			else
				ST_failed("Expected a resource and didn't get one or vice versa");
		}
		else
		{
			ST_failed("Failed to receive the expected addition key");
		}
	}
	else
	{
		ST_failed("Failed to receive the expected addition type");
	}
	OP_DELETE(this);
}

void ST_AdditionChecker::OnAdditionResolveFailed(UpdatableResource::UpdatableResourceType type, const OpStringC& key)
{
	// It shouldn't hurt if we can't remore the listener since the listener is this object
	OpStatus::Ignore(RemoveListener(this));
	ST_failed("Could not resolve the addition!");
	OP_DELETE(this);
}

#endif
#endif
