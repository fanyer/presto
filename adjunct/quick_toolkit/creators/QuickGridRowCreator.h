/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef QUICK_GRID_ROW_CREATOR_H
#define QUICK_GRID_ROW_CREATOR_H

#include "adjunct/ui_parser/ParserNode.h"

class QuickGrid;
class TypedObjectCollection;
class ParserLogger;

class QuickGridRowCreator
{
public:
	/** Create a row creator
	  * @param sequence Sequence node that describes elements for row
	  * @param log Log for errors
	  */
	QuickGridRowCreator(ParserNodeSequence sequence, ParserLogger& log, bool has_quick_window) : m_sequence(sequence), m_log(log), m_has_quick_window(has_quick_window) {}

	/** Instantiate a row
	  * @param grid Grid to instantiate row into
	  * @param collection Where to save named widgets to
	  * @return OpStatus::OK on success, error codes otherwise
	  */
	OP_STATUS Instantiate(QuickGrid& grid, TypedObjectCollection& collection);

private:
	ParserNodeSequence m_sequence;
	ParserLogger& m_log;
	bool m_has_quick_window;
};

#endif // QUICK_GRID_ROW_CREATOR_H
