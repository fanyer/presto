/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "modules/logdoc/src/html5/html5tokenbuffer.h"

void HTML5TokenBuffer::ResetL()
{
	Clear();
	if (m_buffer_length > m_initial_size)
		FreeBuffer();

	EnsureCapacityL(m_initial_size);
}

void HTML5TokenBuffer::TakeOver(HTML5TokenBuffer& take_over_from)
{
	OP_ASSERT(!m_buffer && !m_buffer_length);

	m_buffer = take_over_from.m_buffer;
	m_buffer_length = take_over_from.m_buffer_length;
	m_used = take_over_from.m_used;
	m_hash_base = take_over_from.m_hash_base;
	m_hash_value = take_over_from.m_hash_value;

	// Set the other buffer to be empty
	take_over_from.m_buffer = NULL;
	take_over_from.m_buffer_length = take_over_from.m_used = 0;
}

void HTML5TokenBuffer::CopyL(const HTML5TokenBuffer *copy_from)
{
	uni_char *new_buf = OP_NEWA_L(uni_char, copy_from->m_used + 1);
	op_memcpy(new_buf, copy_from->m_buffer, UNICODE_SIZE(copy_from->m_used));
	OP_DELETEA(m_buffer);
	m_buffer = new_buf;
	m_used = copy_from->m_used;
	m_buffer_length = m_used + 1;
	m_buffer[m_used] = 0; // termination, doesn't count as used.
	m_hash_value = copy_from->m_hash_value;
}

void HTML5TokenBuffer::AppendL(const uni_char *str, unsigned str_len)
{
	EnsureCapacityL(str_len);
	op_memcpy(m_buffer + m_used, str, UNICODE_SIZE(str_len));
	m_used += str_len;
}

void HTML5TokenBuffer::GrowL(unsigned added_length)
{
	unsigned new_size = MAX(m_used + added_length, m_buffer_length + m_initial_size);
	uni_char *new_buf = OP_NEWA_L(uni_char, new_size);

	op_memcpy(new_buf, m_buffer, m_used * sizeof(uni_char));
	OP_DELETEA(m_buffer);
	m_buffer = new_buf;
	m_buffer_length = new_size;
}
