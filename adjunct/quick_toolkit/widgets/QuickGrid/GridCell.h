/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef GRID_CELL_H
#define GRID_CELL_H

#include "adjunct/quick_toolkit/widgets/QuickWidgetContent.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/GenericGrid.h"
#include "adjunct/quick_toolkit/widgets/WidgetSizes.h"

class QuickWidget;

/** @brief A cell in a grid
  */
class GridCell
{
public:
	virtual ~GridCell() {}

	/** @return Number of columns this cell spans
	  */
	virtual int GetColSpan() const { return 1; }

	/** @return Number of rows this cell spans
	  */
	virtual int GetRowSpan() const { return 1; }

	/** Set the content of this grid cell
	  * @param widget Content to set (takes ownership)
	  */
	void SetContent(QuickWidget* widget) { m_contents = widget; }

	/** @return Contents of this widget (always a valid pointer)
	  */
	QuickWidget* GetContent() const { return m_contents.GetContent(); }

	// QuickWidget-like interface that operates on the contained widget (See QuickWidget for documentation)
	void SetContainer(QuickWidgetContainer* container) { m_contents->SetContainer(container); }
	virtual void SetParentOpWidget(class OpWidget* parent) { return m_contents->SetParentOpWidget(parent); }
	virtual OP_STATUS Layout(const OpRect& layout_rect) { return m_contents->Layout(layout_rect); }

	virtual unsigned GetMinimumWidth() { return m_contents->IsVisible() ? m_contents->GetMinimumWidth() : 0; }
	virtual unsigned GetMinimumHeight(unsigned width = WidgetSizes::UseDefault) { return m_contents->IsVisible() ? m_contents->GetMinimumHeight(width) : 0; }
	virtual unsigned GetNominalWidth() { return m_contents->IsVisible() ? m_contents->GetNominalWidth() : 0; }
	virtual unsigned GetNominalHeight(unsigned width = WidgetSizes::UseDefault) { return m_contents->IsVisible() ? m_contents->GetNominalHeight(width) : 0; }
	virtual unsigned GetPreferredWidth() { return m_contents->IsVisible() ? m_contents->GetPreferredWidth() : 0; }
	virtual unsigned GetPreferredHeight(unsigned width = WidgetSizes::UseDefault) { return m_contents->IsVisible() ? m_contents->GetPreferredHeight(width) : 0; }

	op_force_inline WidgetSizes::Margins GetMargins() { return m_contents->IsVisible() ? m_contents->GetMargins() : WidgetSizes::Margins(0); }

private:
	QuickWidgetContent m_contents;
};


/** @brief A cell in a grid that can span multiple rows or columns
  */
class SpannedGridCell : public GridCell
{
public:
	/** Constructor
	  * @param rowspan Number of rows spanned
	  * @param colspan Number of columns spanned
	  */
	SpannedGridCell(int rowspan, int colspan) : m_rowspan(rowspan), m_colspan(colspan) {}

	virtual int GetColSpan() const { return m_colspan; }
	virtual int GetRowSpan() const { return m_rowspan; }

	// Spanned cells do not contribute to the size
	virtual unsigned GetMinimumWidth() { return m_colspan > 1 ? 0 : GridCell::GetMinimumWidth(); }
	virtual unsigned GetMinimumHeight(unsigned width) { return m_rowspan > 1 ? 0 : GridCell::GetMinimumHeight(width); }
	virtual unsigned GetNominalWidth() { return m_colspan > 1 ? 0 : GridCell::GetNominalWidth(); }
	virtual unsigned GetNominalHeight(unsigned width) { return m_rowspan > 1 ? 0 : GridCell::GetNominalHeight(width); }
	virtual unsigned GetPreferredWidth() { return m_colspan > 1 ? 0 : GridCell::GetPreferredWidth(); }
	virtual unsigned GetPreferredHeight(unsigned width) { return m_rowspan > 1 ? 0 : GridCell::GetPreferredHeight(width); }

private:
	int m_rowspan;
	int m_colspan;
};

#endif // GRID_CELL_H
