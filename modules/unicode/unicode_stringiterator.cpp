/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"
#include "modules/unicode/unicode_stringiterator.h"

UnicodeStringIterator::UnicodeStringIterator(const uni_char* str, int start_position /* = 0 */, int length /* = KAll */)
	: m_str(str)
	, m_length(str ? (length < 0 ? uni_strlen(str) : static_cast<size_t>(length)) : 0)
	, m_pos(start_position < 0 || static_cast<size_t>(start_position) > m_length
			? m_length : start_position)
	, m_pos_delta(0)
	, m_index(m_pos)
	, m_current(0)
{
	SetNextUnicodePoint();
}

UnicodeStringIterator::UnicodeStringIterator(const UniString& string, size_t start_position /* = 0 */)
	: m_itr(string, start_position < string.Length())
	, m_str(m_itr.GetData())
	, m_length(m_itr.GetLength())
	, m_pos(0)
	, m_pos_delta(0)
	, m_index(0)
	, m_current('\0')
{
	if (start_position >= string.Length())
	{	/* No need to iterate to the end, m_itr already points to the end,
		 * m_pos_delta and m_current are already 0, so we only need to adjust
		 * the m_pos and m_index. */
		m_pos = m_length;
		m_index = string.Length();
	}
	else
	{	/* iterate m_itr to the fragment that contains the requested start
		 * position: */
		while (m_index < start_position)
		{
			if (m_index + m_length-1 >= start_position)
			{	// found the fragment
				m_pos = start_position-m_index;
				m_index = start_position;
				OP_ASSERT(IsIteratingUniString());
			}
			else
			{	// move on to the next fragment
				m_index += m_length;
				StepToNextFragment();
			}
		}
		SetNextUnicodePoint();
	}
}

bool UnicodeStringIterator::Next()
{
	/* Move m_index after the current unicode-point and let m_pos point to the
	 * same character as m_index. (Note: if we are already at the end of the
	 * string then m_pos_delta == 0 and we don't move m_index nor m_pos.) */
	m_index += op_abs(m_pos_delta);
	while (m_pos_delta < 0)
		NextUniChar();
	m_pos_delta = 0;

	// Try to move on to the next unicode-point
	SetNextUnicodePoint();

	// m_pos_delta == 0 if we are at the end of the string,
	// m_pos_delta == 1 if m_current uses one uni_char
	// m_pos_delta == 2 if m_current uses two uni_char
	OP_ASSERT(m_pos_delta == 0 || m_pos_delta == 1 || m_pos_delta == 2);
	return m_pos_delta > 0;
}

bool UnicodeStringIterator::Previous()
{
	if (m_index == 0)
		return false;
	OP_ASSERT(m_index > 0);

	// let m_pos point to the same character as m_index:
	while (m_pos_delta > 0)
		PrevUniChar();
	m_pos_delta = 0;

	uni_char low = PrevUniChar();
	if (Unicode::IsLowSurrogate(low) && m_index > 1)
	{
		uni_char prev = PrevUniChar();
		if (Unicode::IsHighSurrogate(prev))
		{	// the unicode-point uses two uni_char
			OP_ASSERT(m_pos_delta = -2);
			OP_ASSERT(m_pos < m_length);
			OP_ASSERT(m_index >= 2);
			m_current = Unicode::DecodeSurrogate(prev, low);
			m_index -= 2;
			return true;
		}
		else
		{	/* don't use the prev character, so move ahead the m_pos pointer,
			 * which may now be equal to m_length: */
			++m_pos;
			++m_pos_delta;
		}
	}
	// the unicode-point is one uni_char
	OP_ASSERT(m_pos_delta == -1);
	OP_ASSERT(m_pos < m_length);
	OP_ASSERT(m_index >= 1);
	m_current = low;
	m_index--;
	return true;
}

void UnicodeStringIterator::Reset()
{
	if (IsIteratingUniString())
	{
		// Reset iterator if iterating over a UniString
		m_itr.Reset();
		m_str = m_itr.GetData();
		m_length = m_itr.GetLength();
	}
	m_index = m_pos = m_pos_delta = 0;
	m_current = '\0';
	SetNextUnicodePoint();
}

UnicodeStringIterator UnicodeStringIterator::FromBeginning() const
{
	UnicodeStringIterator newit(*this);
	newit.Reset();
	return newit;
}

void UnicodeStringIterator::SetNextUnicodePoint()
{
	OP_ASSERT(m_pos_delta == 0);
	uni_char next = NextUniChar();
	if (Unicode::IsHighSurrogate(next))
	{
		uni_char low = NextUniChar();
		if (Unicode::IsLowSurrogate(low))
		{
			m_current = Unicode::DecodeSurrogate(next, low);
			OP_ASSERT(m_pos_delta == 2);
		}
		else
		{
			m_current = next;
			if (m_pos_delta == 2)
			{	/* If the last NextUniChar() moved the iterator one character
				 * ahead, but we don't use that character, we have to move m_pos
				 * and m_pos_delta back again. */
				OP_ASSERT(m_pos > 0);
				--m_pos;
				--m_pos_delta;
			}
			OP_ASSERT(m_pos_delta == 1);
		}
	}
	else
		m_current = next;
}

void UnicodeStringIterator::StepToNextFragment()
{
	OP_ASSERT(m_itr.HasNext());
	m_length = ++m_itr ? m_itr.GetLength() : 0;
	m_str = m_itr.GetData();
	m_pos = 0;
	OP_ASSERT(m_itr.IsValid() && m_str != NULL && m_length > 0);
}

void UnicodeStringIterator::StepToPrevFragment()
{
	OP_ASSERT(m_itr.HasPrev());
	m_length = --m_itr ? m_itr.GetLength() : 0;
	m_str = m_itr.GetData();
	m_pos = m_length > 0 ? m_length-1 : 0;
	OP_ASSERT(m_itr.IsValid());
}

uni_char UnicodeStringIterator::NextUniChar()
{
	if (m_pos == m_length)
	{
		if (m_itr.HasNext())
			StepToNextFragment();
		else
		{	/* We should never get here when Next() calls this method to move
			 * the m_pos to the same character as m_index points to (i.e., when
			 * being called in the while (m_pos_delta < 0) loop in method
			 * Previous()). */
			OP_ASSERT(m_pos_delta >= 0);
			return 0;
		}
	}
	OP_ASSERT(m_pos < m_length);
	m_pos_delta++;
	return m_str[m_pos++];
}

uni_char UnicodeStringIterator::PrevUniChar()
{
	if (m_pos == 0)
	{
		if (m_itr.HasPrev())
			StepToPrevFragment();
		else
		{	/* We should never get here when Previous() calls this method to
			 * move the m_pos to the same character as m_index points to
			 * (i.e., when being called in the while (m_pos_delta > 0) loop
			 * in method Previous()).  */
			OP_ASSERT(m_pos_delta <= 0);
			return 0;
		}
	}
	else
	{
		OP_ASSERT(m_pos > 0);
		--m_pos;
	}
	--m_pos_delta;
	return m_str[m_pos];
}

