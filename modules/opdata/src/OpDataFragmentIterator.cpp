/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"
#include "modules/opdata/OpDataFragmentIterator.h"

void OpDataCharIterator::MoveToIndex(size_t index)
{
	OP_ASSERT(m_index <= index);
	while (m_index < index)
	{
		if (m_index + m_current_length-1 >= index)
		{
			m_current_index = index-m_index;
			m_index = index;
			OP_ASSERT(IsValid());
		}
		else
		{
			m_index += m_current_length;
			StepToNextFragment();
		}
	}
}

bool OpDataCharIterator::NextWithFragment()
{
	OP_ASSERT(m_current_index+1 >= m_current_length);
	if (IsAtEnd())
		return false;
	OP_ASSERT(m_itr.IsValid() && m_current_index < m_current_length);
	++m_index;
	if (++m_current_index == m_current_length)
	{
		if (!m_itr.HasNext())
			return false; // we hit the end of the buffer
		StepToNextFragment();
	}
	return true;
}

void OpDataCharIterator::StepToNextFragment()
{
	OP_ASSERT(m_itr.HasNext());
	++m_itr;
	m_current_length = m_itr.GetLength();
	m_current_data = m_itr.GetData();
	m_current_index = 0;
	OP_ASSERT(m_itr.IsValid() && m_current_index < m_current_length);
}

bool OpDataCharIterator::PreviousWithFragment()
{
	OP_ASSERT(m_current_index == 0);
	if (m_index == 0)
		return false;

	OP_ASSERT(IsValid());
	OP_ASSERT(m_itr.HasPrev());

	--m_index;
	--m_itr;
	m_current_data = m_itr.GetData();
	OP_ASSERT(m_current_data);
	m_current_length = m_itr.GetLength();
	m_current_index = m_current_length-1;
	OP_ASSERT(IsValid());
	return true;
}
