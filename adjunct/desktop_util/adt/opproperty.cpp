/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/desktop_util/adt/opproperty.h"

#include "modules/util/adt/opvector.h"

BOOL OpPropertyFunctions::Equals(const OpINT32Vector& lhs, const OpINT32Vector& rhs)
{
	if (lhs.GetCount() != rhs.GetCount())
	{
		return FALSE;
	}

	for (UINT32 i = 0; i < rhs.GetCount(); ++i)
	{
		if (lhs.Get(i) != rhs.Get(i))
		{
			return FALSE;
		}
	}

	return TRUE;
}

void OpPropertyFunctions::Assign(OpINT32Vector& lhs, const OpINT32Vector& rhs)
{
	ReportIfError(lhs.DuplicateOf(rhs));
}
