/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "modules/logdoc/src/html5/html5token.h"
#include "modules/logdoc/html5parser.h" // for IsHTML5WhiteSpace
#include "modules/logdoc/src/html5/elementhashbase.h" // for HTML5_TAG_HASH_BASE

void HTML5Token::InitializeL(unsigned init_name_len/*=128*/, unsigned init_attr_len/*=HTML5Token::kAttrChunkSize*/)
{
	// don't allocate space if we are going to copy it to another buffer anyway
	// since the copy process will delete the old buffers and allocate new ones
	if (init_name_len)
		m_name.EnsureCapacityL(init_name_len);
	m_name.SetHashBase(HTML5_TAG_HASH_BASE);

	if (init_attr_len > m_attributes_length)
	{
		OP_DELETEA(m_attributes);
		m_attributes = OP_NEWA_L(HTML5Attr, init_attr_len);
		m_attributes_length = init_attr_len;
	}
}

BOOL HTML5Token::IsWSOnly(BOOL include_null_char) const
{
	HTML5StringBuffer::ContentIterator iter(m_data);
	unsigned length;
	const uni_char* pos = iter.GetNext(length);
	while (pos)
	{
		for (; length; length--)
		{
			if (!HTML5Parser::IsHTML5WhiteSpace(*pos) && (!include_null_char || *pos != 0))
				return FALSE;
			pos++;
		}
		pos = iter.GetNext(length);
	}

	return TRUE;
}

HTML5Attr* HTML5Token::GetNewAttributeL()
{
	if (m_last_attribute_is_valid && m_number_of_attributes + 1 > m_attributes_length)
	{
		unsigned new_length = m_attributes_length + HTML5Token::kAttrChunkSize;
		HTML5Attr* new_attr = OP_NEWA_L(HTML5Attr, new_length);
		
		for (unsigned i = 0; i < m_number_of_attributes; i++)
			new_attr[i].TakeOver(m_attributes[i]);
			
		OP_DELETEA(m_attributes);
		m_attributes = new_attr;
		
		m_attributes_length = new_length;
	}
	else  // reuse an old unused attribute
		m_attributes[m_number_of_attributes].Clear();

	m_last_attribute_is_valid = TRUE;
	
	return &m_attributes[m_number_of_attributes++];
}

BOOL HTML5Token::CheckIfLastAttributeIsDuplicate()
{
	OP_ASSERT(m_last_attribute_is_valid);
	OP_ASSERT(m_number_of_attributes >= 1);

	const HTML5TokenBuffer *last_attr = m_attributes[m_number_of_attributes - 1].GetName();
	const uni_char* last_attr_name = last_attr->GetBuffer();
	unsigned last_attr_name_length = last_attr->Length();

	for (unsigned i = 0; i < m_number_of_attributes - 1; i++)
	{
		if (m_attributes[i].GetName()->IsEqualTo(last_attr_name, last_attr_name_length))
		{
			m_last_attribute_is_valid = FALSE;
			m_number_of_attributes--;

			return TRUE;
		}
	}

	return FALSE;
}
