/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef GADGETS_HOTLIST_CONTROLLER_H
#define GADGETS_HOTLIST_CONTROLLER_H

#ifdef WIDGET_RUNTIME_SUPPORT

class OpInputAction;
class OpTreeView;


/**
 * The controller for actions invoked on the gadgets panel.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class GadgetsHotlistController
{
public:
	/**
	 * Constructs the controller and establishes the associated with the view.
	 *
	 * @param item_view the tree view of GadgetsTreeModelItems
	 */
	explicit GadgetsHotlistController(OpTreeView& item_view);

	/**
	 * Handles an action.
	 *
	 * @param action the action
	 * @return @c TRUE iff the action has been handled
	 */
	BOOL HandleAction(OpInputAction& action);

private:
	OpTreeView* m_item_view;
};


#endif // WIDGET_RUNTIME_SUPPORT
#endif // GADGETS_HOTLIST_CONTROLLER_H
