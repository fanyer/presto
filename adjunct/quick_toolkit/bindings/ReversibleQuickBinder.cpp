/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/bindings/ReversibleQuickBinder.h"


ReversibleQuickBinder::ReversibleQuickBinder()
	: m_committed(false)
{
}

ReversibleQuickBinder::~ReversibleQuickBinder()
{
	if (!m_committed)
	{
		RevertAll();
	}
}

OP_STATUS ReversibleQuickBinder::AddReverter(PrefUtils::IntegerAccessor& accessor)
{
	OpAutoPtr<IntegerPrefReverter> reverter(OP_NEW(IntegerPrefReverter, (accessor)));
	RETURN_OOM_IF_NULL(reverter.get());
	RETURN_IF_ERROR(m_reverters.Add(reverter.get()));
	reverter.release();
	return OpStatus::OK;
}

OP_STATUS ReversibleQuickBinder::AddReverter(PrefUtils::StringAccessor& accessor)
{
	OpAutoPtr<StringPrefReverter> reverter(OP_NEW(StringPrefReverter, (accessor)));
	RETURN_OOM_IF_NULL(reverter.get());
	RETURN_IF_ERROR(reverter->Init());
	RETURN_IF_ERROR(m_reverters.Add(reverter.get()));
	reverter.release();
	return OpStatus::OK;
}

void ReversibleQuickBinder::Commit()
{
	m_committed = true;
}

void ReversibleQuickBinder::RevertAll()
{
	for (UINT32 i = 0; i < m_reverters.GetCount(); ++i)
	{
		m_reverters.Get(i)->Revert();
	}
}
