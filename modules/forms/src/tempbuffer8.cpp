/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/forms/tempbuffer8.h"

#include "modules/formats/uri_escape.h" // For escaping

/**
 * Either we encode SPACE as %20 or as +. This says that we should use +.
 */
#define _TEMPBUFFER8_USE_PLUS_FOR_SPACE_

/**
 * This is a scaled down version of the TempBuffer changed to use 8
 * bit chars instead of uni_char.
 */

OP_STATUS TempBuffer8::EnsureSize(size_t required_size)
{
	if (required_size > m_storage_size)
	{
		// Attempt different sizes until we try the exact size and then fail
		// if we OOM. The smaller size we try, the worse performance we get.
		size_t new_size = m_storage_size;
#ifdef CONSERVATIVE_ALLOCATION
		size_t margin = 1; // Waste no memory, will be "halved" to zero in the loop
#else // Grow with a margin to get better than O(n^2) performance
		do
		{
			new_size = MAX(new_size + 128, new_size << 1);
		} while (new_size < required_size);

		size_t margin = (new_size - required_size) * 2; // Will be halved in the loop
		OP_ASSERT(margin / 2 == new_size - required_size); // no overflow
#endif // CONSERVATIVE_ALLOCATION
		char* new_buf = NULL;
		do
		{
			margin = margin >> 1;
			new_size = required_size + margin;
			new_buf = OP_NEWA(char, new_size);
		}
		while (!new_buf && margin > 0);

		OP_ASSERT(margin == 0 || new_buf);

		if (!new_buf)
		{
			return OpStatus::ERR_NO_MEMORY;
		}

		// We have a new buffer. Copy the old content to it.
		if (m_str_len)
		{
			OP_ASSERT(m_storage);
			op_memcpy(new_buf, m_storage, m_str_len + 1);
		}
		else
		{
			*new_buf = '\0';
		}

		DeleteBuffer(); // Deletes the previous buffer contents
		m_storage = new_buf;
		m_storage_size = new_size;
	}

	OP_ASSERT(required_size <= m_storage_size);
	return OpStatus::OK;
}

OP_STATUS TempBuffer8::Append(const char* str, unsigned length)
{
	OP_ASSERT(str);

	size_t len = 0;
	const char *q = str;
	while (len < length && *q)
	{
		++q; ++len;
	}
	RETURN_IF_ERROR(EnsureSize(m_str_len + len + 1));

	op_memcpy(m_storage + m_str_len, str, len);

	m_str_len += len;
	m_storage[m_str_len] = 0;

	return OpStatus::OK;
}

OP_STATUS TempBuffer8::AppendURLEncoded(const char* val, BOOL use_plus_for_space, BOOL is_form_submit)
{
	size_t val_len = StrLenURLEncoded(val, use_plus_for_space, is_form_submit);
	OP_ASSERT(val_len >= op_strlen(val));

	RETURN_IF_ERROR(EnsureSize(m_str_len + val_len + 1));

	int escape_flags = UriEscape::AllUnsafe;
	if (is_form_submit)
		escape_flags = UriEscape::FormUnsafe;
#ifdef _TEMPBUFFER8_USE_PLUS_FOR_SPACE_
	if (use_plus_for_space)
		escape_flags |= UriEscape::UsePlusForSpace;
#endif

	UriEscape::Escape(m_storage + m_str_len, val, escape_flags);
	m_str_len += val_len;

	return OpStatus::OK;
}

/* static */
size_t TempBuffer8::StrLenURLEncoded(const char* str, BOOL use_plus_for_space, BOOL is_form_submit)
{
	int escape_flags = UriEscape::AllUnsafe;
	if (is_form_submit)
		escape_flags = UriEscape::FormUnsafe;
#ifdef _TEMPBUFFER8_USE_PLUS_FOR_SPACE_
	if (use_plus_for_space)
		escape_flags |= UriEscape::UsePlusForSpace;
#endif

	return UriEscape::GetEscapedLength(str, escape_flags);
}

#ifdef CLEAR_PASSWD_FROM_MEMORY
void TempBuffer8::DeleteBuffer()
{
	if (m_storage)
	{
		if (m_str_len)
		{
			op_memset(m_storage, 0, m_str_len);
		}
		OP_DELETEA(m_storage);
	}
}
#endif // CLEAR_PASSWD_FROM_MEMORY
