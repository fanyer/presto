/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#include "strutil.h"
#include "adjunct/desktop_util/string/stringutils.h"


//****************************************************************************
//
//	StringTokenizer
//
//****************************************************************************

StringTokenizer::StringTokenizer(const OpStringC& input,
	const OpStringC &tokens)
:	m_tokens(tokens)
{
	Init(input);
}


OP_STATUS StringTokenizer::Init(const OpStringC& input)
{
	RETURN_IF_ERROR(m_input_string.Set(input));
	RETURN_IF_ERROR(StringUtils::Strip(m_input_string, m_tokens));

	return OpStatus::OK;
}


BOOL StringTokenizer::HasMoreTokens() const
{
	BOOL have_more = FALSE;

	if (m_input_string.FindFirstOf(m_tokens) != KNotFound)
		have_more = TRUE;
	else if (m_input_string.HasContent())
		have_more = TRUE;

	return have_more;
}


OP_STATUS StringTokenizer::NextToken(OpString &token)
{
	if (!HasMoreTokens())
		return OpStatus::ERR;

	const int find_pos = m_input_string.FindFirstOf(m_tokens);
	if (find_pos != KNotFound)
		RETURN_IF_ERROR(token.Set(m_input_string.CStr(), find_pos));
	else
		RETURN_IF_ERROR(token.Set(m_input_string));

	m_input_string.Delete(0, find_pos != KNotFound ? find_pos + 1 : KAll);
	RETURN_IF_ERROR(StringUtils::Strip(m_input_string, m_tokens));

	return OpStatus::OK;
}


OP_STATUS StringTokenizer::SkipToken()
{
	OpString token;
	return NextToken(token);
}


OP_STATUS StringTokenizer::RemainingString(OpString &string) const
{
	RETURN_IF_ERROR(string.Set(m_input_string));
	RETURN_IF_ERROR(StringUtils::Strip(string, m_tokens));

	return OpStatus::OK;
}


#endif // M2_SUPPORT
