/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef GADGET_TOOLTIP_HANDLER_H
#define GADGET_TOOLTIP_HANDLER_H

#ifdef WIDGET_RUNTIME_SUPPORT

class OpToolTip;
class OpToolTipListener;

/**
 * A tooltip handler for gadgets.
 *
 * Sports a simple show/hide interface.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class GadgetTooltipHandler
{
public:
	GadgetTooltipHandler();
	~GadgetTooltipHandler();

	OP_STATUS Init();

	/**
	 * Displays a tooltip in the gadget.
	 *
	 * The position is calculated automatically based on the mouse pointer
	 * position.
	 *
	 * @param link the address to be displayed in the tooltip (may be empty)
	 * @param title the title to be displayed in the tooltip (may be empty)
	 * @return status
	 */
	OP_STATUS ShowTooltip(const OpStringC& link, const OpStringC& title);

	/**
	 * Hides the tooltip currently being displayed.  Has no effect if no tooltip
	 * is being displayed.
	 *
	 * @return status
	 */
	OP_STATUS HideTooltip();

protected:
#ifdef SELFTEST
	OpToolTipListener* GetTooltipListener() const;
#endif // SELFTEST

private:
	class TooltipListener;

	void CleanUp();

	OpToolTip* m_tooltip;
	TooltipListener* m_listener;
};

#endif // WIDGET_RUNTIME_SUPPORT
#endif // GADGET_TOOLTIP_HANDLER_H
