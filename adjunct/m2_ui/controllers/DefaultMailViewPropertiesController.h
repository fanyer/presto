/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Alexander Remen (alexr@opera.com)
 */

#ifndef DEFAULT_MAIL_VIEW_PROPERTIES_CONTROLLER_H
#define DEFAULT_MAIL_VIEW_PROPERTIES_CONTROLLER_H

#ifdef M2_SUPPORT

#include "adjunct/quick_toolkit/contexts/PopupDialogContext.h"

class MailDesktopWindow;
class QuickOverlayDialog;

class DefaultMailViewPropertiesController : public PopupDialogContext
{
public:
	DefaultMailViewPropertiesController(OpWidget* anchor_widget, MailDesktopWindow* mail_window);

	// UiContext
	virtual BOOL			CanHandleAction(OpInputAction* action);
	virtual BOOL			DisablesAction(OpInputAction* action);
	virtual BOOL			SelectsAction(OpInputAction* action);
	virtual OP_STATUS		HandleAction(OpInputAction* action);

private:
	class DialogPlacerUpdater
	{
	public:
		DialogPlacerUpdater(DefaultMailViewPropertiesController* controller);
		~DialogPlacerUpdater();

	private:
		DefaultMailViewPropertiesController* m_controller;
	};
	friend class DialogPlacerUpdater;

	virtual void InitL();

	MailDesktopWindow*			m_mail_window;
	QuickOverlayDialog*			m_overlay_dialog;
};


#endif // M2_SUPPORT
#endif // DEFAULT_MAIL_VIEW_PROPERTIES_CONTROLLER_H
