/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Alexander Remen (alexr)
 */

// Class that maps lines to the items that own them
#ifndef LINE_TO_ITEM_MAP_H
#define LINE_TO_ITEM_MAP_H

#include "modules/util/adt/opvector.h"

class LineToItemMap 
{
public:
					/**
					 *	Adds an item spanning @a num_lines lines and
					 *	automatically increases all values after that item with
					 *	1.
					 */
	OP_STATUS		AddItem(INT32 new_value, UINT32 num_lines = 1);
					
					/**
					 * Adds lines with a value, but doesn't increase all values after those lines
					 */
	OP_STATUS		InsertLines(INT32 new_value, UINT32 num_lines);

	void			BeginInsertion() { m_insertion_mode = true; m_num_lines = m_end_index = 0; }
	OP_STATUS		InsertItem(INT32 new_value, UINT32 num_lines);
	void			EndInsertion();

					/**
					 * Removes items in the range [value_start .. value_end]
					 */
	void			RemoveItems(INT32 value_start, INT32 value_end) { RemoveLines(value_start, value_end, value_end - value_start + 1); }
					
					/**
					 * Removes lines, but doesn't decrease all values after those lines
					 */
	void			RemoveLines(INT32 value) { RemoveLines(value, value, 0); }
	void			RemoveLines(INT32 value_start, INT32 value_end) { RemoveLines(value_start, value_end, 0); }
	
	void			Clear() { m_lines.Clear(); }

	UINT32			GetCount() const { return m_lines.GetCount(); }

	INT32			GetByIndex(UINT32 index) const;

	INT32			Find(INT32 item);

private:
	void			RemoveLines(INT32 index, INT32 count, INT32 offset);

	INT32			BinarySearch(INT32 item_to_find);

	OpINT32Vector	m_lines;
	bool			m_insertion_mode;
	INT32			m_num_lines;
	INT32			m_end_index;
};

#endif // LINE_TO_ITEM_MAP_H
