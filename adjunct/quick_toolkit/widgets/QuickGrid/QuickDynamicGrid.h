/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef QUICK_DYNAMIC_GRID_H
#define QUICK_DYNAMIC_GRID_H

#include "adjunct/quick_toolkit/widgets/QuickGrid/QuickGrid.h"

class OpWidget;
class QuickGridRowCreator;

/** @brief Grid with contents that might change over time
  * This grid class can be assigned a template for generating
  * new rows, and it has special properties related to testability
  * so that it can be recognized by the testing framework as a
  * grid with dynamic content
  */
class QuickDynamicGrid : public QuickGrid
{
	IMPLEMENT_TYPEDOBJECT(QuickGrid);
public:
	enum TestState
	{
		NORMAL,			///< Use Opera settings
		FORCE_TESTABLE,	///< Force testability properties on
	};

	QuickDynamicGrid();
	~QuickDynamicGrid();

	OP_STATUS Init(TestState teststate = NORMAL);

	/** Set a name for this widget (used in testability context)
	  * @param name Name to use
	  */
	void SetName(const OpStringC8& name);
	
	/** Set a template instantiator for this widget
	 * @param instantiator Class used to instantiate templates (takes ownership)
	 */
	void SetTemplateInstantiator(QuickGridRowCreator* instantiator);
	
	/** Instantiate from the template. Only works after calling SetTemplateInstantiator
	 */
	OP_STATUS Instantiate(TypedObjectCollection& collection);

	// Override QuickGrid
	virtual void SetParentOpWidget(OpWidget* parent);
	virtual OP_STATUS Layout(const OpRect& rect);
	virtual GridLayouter* CreateLayouter();

private:
	// Override QuickGrid
	virtual OP_STATUS OnRowAdded();
	virtual void OnRowRemoved(unsigned row);
	virtual void OnRowMoved(unsigned from_row, unsigned to_row);
	virtual void OnCleared();

	class BackgroundWidget;
	class ItemWidget;
	class TestLayouter;

	BackgroundWidget* m_background;
	QuickGridRowCreator* m_instantiator;
};

#endif // QUICK_DYNAMIC_GRID_H
