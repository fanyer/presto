/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/desktop_util/algorithms/opunique.h"

const uni_char OpUnique::DEFAULT_SUFFIX_FORMAT[] = UNI_L(" (%d)");

OpUnique::OpUnique(OpString& name, const OpStringC& suffix_format)
	: m_name(&name)
	, m_suffix_format(suffix_format)
	, m_counter(2)
{}

OP_STATUS OpUnique::Next()
{
	if (m_counter == 2)
		RETURN_IF_ERROR(m_original_name.Set(*m_name));

	RETURN_IF_ERROR(m_name->Set(m_original_name));
	RETURN_IF_ERROR(m_name->AppendFormat(m_suffix_format.CStr(), m_counter));
	++m_counter;

	return OpStatus::OK;
}
