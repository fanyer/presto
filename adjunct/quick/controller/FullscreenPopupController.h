/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 */

#ifndef FULLSCREEN_POPUP_CONTROLLER_H
#define FULLSCREEN_POPUP_CONTROLLER_H

#include "adjunct/quick_toolkit/contexts/PopupDialogContext.h"
#include "adjunct/quick_toolkit/widgets/QuickOverlayDialog.h"


/**
 * @brief Shows a popup notification when going into fullscreen that tells you how to get out of that mode.
 */
class FullscreenPopupController
		: public PopupDialogContext
		, private OpWidgetListener
		, public OpTimerListener
{
public:
	FullscreenPopupController(OpWidget* anchor_widget);

	void Show();

	void CancelDialog();

	// OpTimerListener
	virtual void OnTimeOut(OpTimer* timer);

	// PopupDialogContext
	virtual OP_STATUS HandleAction(OpInputAction* action);
	virtual BOOL CanHandleAction(OpInputAction* action);

private:
	static const unsigned DELAY_BEFORE_OPENING =  200; // in milliseconds
	static const unsigned DELAY_BEFORE_CLOSING = 3000;

	class FullscreenPopupPlacer : public QuickOverlayDialogPlacer
	{
	public:
		virtual OpRect CalculatePlacement(const OpRect& bounds, const OpRect& dialog_size)
		{
			return OpRect((bounds.width - dialog_size.width) / 2,
						  (bounds.height - dialog_size.height) / 2,
						  dialog_size.width, dialog_size.height);
		}
		virtual void PointArrow(OpWidgetImage& arrow_skin, const OpRect& placement) {}
		virtual BOOL UsesRootCoordinates() { return TRUE; }
	};

	// PopupDialogContext
	virtual void			InitL();

	OpTimer					m_timer;	// for delaying showing and hiding the popup
	bool					m_visible;	// true when the popup is visible and waiting to be hidden
};

#endif //FULLSCREEN_POPUP_CONTROLLER_H
