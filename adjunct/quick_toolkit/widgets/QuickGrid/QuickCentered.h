/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef QUICK_CENTERED_H
#define QUICK_CENTERED_H

#include "adjunct/quick_toolkit/widgets/QuickGrid/GenericGrid.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/GridCell.h"

/** @brief A wrapper that centers another widget vertically and horizontally
  * See below for centering just horizontally or just vertically
  */
class QuickCentered : public GenericGrid
{
	IMPLEMENT_TYPEDOBJECT(GenericGrid);
public:

	OP_STATUS Init() { return OpStatus::OK; }

	/** Set the contents to be centered
	  * @param content Contents to be centered (takes ownership)
	  */
	void SetContent(QuickWidget* content)
		{ m_content_cell.SetContent(content); content->SetContainer(this); BroadcastContentsChanged(); }

	/** @return Contents being centered
	  */
	QuickWidget* GetContent() const { return m_content_cell.GetContent(); }

protected:
	class FillCell : public GridCell
	{
		virtual unsigned GetPreferredWidth() { return WidgetSizes::Fill; }
		virtual unsigned GetPreferredHeight(unsigned width) { return WidgetSizes::Fill; }
	};

	// Implement GridCellContainer
	virtual unsigned GetColumnCount() const { return 3; }
	virtual unsigned GetRowCount() const { return 3; }
	virtual GridCell* GetCell(unsigned col, unsigned row) { return (col == 1 && row == 1 ? &m_content_cell : &m_fill_cell); }

	GridCell m_content_cell;
	FillCell m_fill_cell;
};

class QuickCenteredHorizontal : public QuickCentered
{
	// Implement GridCellContainer
	virtual unsigned GetRowCount() const { return 1; }
	virtual GridCell* GetCell(unsigned col, unsigned row) { return (col == 1 && row == 0 ? &m_content_cell : &m_fill_cell); }
};

class QuickCenteredVertical : public QuickCentered
{
	// Implement GridCellContainer
	virtual unsigned GetColumnCount() const { return 1; }
	virtual GridCell* GetCell(unsigned col, unsigned row) { return (col == 0 && row == 1 ? &m_content_cell : &m_fill_cell); }
};

#endif // QUICK_CENTERED_H
