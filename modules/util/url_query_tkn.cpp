/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef URL_QUERY_TOKENIZER_SUPPORT

#include "modules/util/url_query_tkn.h"

void UrlQueryTokenizer::Set(const uni_char* subject)
{
	m_str = subject;
	m_next_amp = subject;
	m_next_equal = NULL;
	m_name_length = 0;
	m_value_length = 0;
	
	MoveNext();
}

void UrlQueryTokenizer::MoveNext()
{
	m_str = m_next_amp;
	
	if (!HasContent())
	{
		m_next_amp = m_next_equal = NULL;
		m_name_length = m_value_length = 0;
		return;
	}
	
	m_next_amp = uni_strchr(m_str, (uni_char)'&');
	m_next_equal = uni_strchr(m_str, (uni_char)'=');
	
	if (m_next_amp && m_next_equal > m_next_amp)
		m_next_equal = NULL;
	
	if (m_next_amp) m_next_amp++;
	if (m_next_equal) m_next_equal++;
	
	m_name_length = m_next_equal || m_next_amp ? nvl(m_next_equal, m_next_amp) - 1 - m_str : uni_strlen(m_str);
	m_value_length = m_next_equal ? (m_next_amp ? m_next_amp - m_next_equal - 1 : uni_strlen(m_next_equal)) : 0;
	
	if (m_name_length == 0)
		MoveNext();
}

#endif //#ifdef URL_QUERY_TOKENIZER_SUPPORT


