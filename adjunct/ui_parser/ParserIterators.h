/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef PARSER_ITERATORS_H
#define PARSER_ITERATORS_H

#include "adjunct/ui_parser/ParserDocument.h"

class ParserNode;
class ParserNodeSequence;

/////////////////////////////////////////////////
 /*
 * @brief An iterator iterating over a ParserNode of type sequence
 * Typical usage:
 *
 * for (ParserSequenceIterator(node) it; it; ++it)
 * {
 *     // Do stuff with it.Get()
 * }
 */
class ParserSequenceIterator
{
public:
	explicit ParserSequenceIterator(ParserNodeSequence& sequence_node);

	void Next();
	ParserNodeID Get() const { return *m_current; }

	operator BOOL() const
		{ return m_current != 0; }

	ParserSequenceIterator& operator++()
		{ Next(); return *this; }

private:
	ParserNodeSequence& m_sequence_node;
	ParserNodeID*	m_current;
};

#endif // PARSER_ITERATORS_H
