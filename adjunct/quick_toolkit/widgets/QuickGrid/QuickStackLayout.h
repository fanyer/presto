/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 * @author Cihat Imamoglu (cihati)
 */

#ifndef OP_STACKLAYOUT_H
#define OP_STACKLAYOUT_H

#include "adjunct/quick_toolkit/widgets/QuickGrid/GenericGrid.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/GridCell.h"
#include "adjunct/desktop_util/adt/optightvector.h"

class QuickRadioButton;

/** @brief A layouting widget in the form of a one-dimensional stack of elements.
  * Can be horizontal or vertical.
 */
class QuickStackLayout : public GenericGrid
{
	IMPLEMENT_TYPEDOBJECT(GenericGrid);
public:
	enum Orientation
	{
		HORIZONTAL,
		VERTICAL
	};

	/** Constructor
	  * @param orientation Horizontal or vertical
	  */
	QuickStackLayout(Orientation orientation);

	virtual ~QuickStackLayout();

	/** Inserts a widget into the stack.
	  * If the widget is a QuickRadioButton, makes it a part of a radio group.
	  * @param widget Pointer to the widget to be added (takes ownership)
	  * @param pos The position to add. Default position is the end of the stack
	  */
	OP_STATUS InsertWidget(QuickWidget* widget, int pos = -1);

	/** Add an empty widget to the stack with specified layout properties
	  * @param min_width Minimum width of the cell
	  * @param min_height Minimum height of the cell
	  * @param pref_width Preferred width of the cell
	  * @param pref_height Preferred height of the cell
	  * @param pos The position to add. Default position is the end of the stack
	  */
	OP_STATUS InsertEmptyWidget(unsigned min_width, unsigned min_height, unsigned pref_width, unsigned pref_height, int pos = -1);

	/** Remove and deallocate a widget at a certain position in the stack
	  * @param pos Position of widget to remove
	  */
	void RemoveWidget(int pos);

	/** Remove and deallocate all widgets from the stack.
	  */
	void RemoveAllWidgets();

protected:
	// Implement GridCellContainer
	virtual unsigned GetColumnCount() const;
	virtual unsigned GetRowCount() const;
	virtual GridCell* GetCell(unsigned col, unsigned row);

private:
	Orientation m_orientation;
	OpTightVector<GridCell*> m_cells;
};

#endif // OP_STACKLAYOUT_H
