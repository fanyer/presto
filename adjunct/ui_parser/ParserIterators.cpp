/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#include "core/pch.h"

#include "adjunct/ui_parser/ParserIterators.h"


////////// ParserSequenceIterator constructor
ParserSequenceIterator::ParserSequenceIterator(ParserNodeSequence& sequence_node)
	: m_sequence_node(sequence_node)
	, m_current(sequence_node.IsEmpty() ? 0 : sequence_node.m_node->data.sequence.items.start)
{
}


////////// Next
void
ParserSequenceIterator::Next()
{
	m_current++;
	if (m_current >= m_sequence_node.m_node->data.sequence.items.top)
		m_current = 0;
}
