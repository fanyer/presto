/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
*
* Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
*
* This file is part of the Opera web browser.	It may not be distributed
* under any circumstances.
*/

#ifndef URL_FAV_MENU_BUTTON_H
#define URL_FAV_MENU_BUTTON_H

#include "modules/widgets/OpButton.h"

#include "adjunct/quick_toolkit/contexts/DialogContextListener.h"

class DialogContext;

class URLFavMenuButton : public OpButton

{
public:
	URLFavMenuButton();

	OP_STATUS					Init(const char *brdr_skin = NULL, const char *fg_skin = NULL);

	void						SetDialogContext(DialogContext* context) { m_dialog_context = context; }

protected:
	// OpWidget
	virtual void				GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);

	// OpButton
	virtual OP_STATUS			GetText(OpString& text);

	// OpInputContext
	virtual BOOL				IsInputContextAvailable(FOCUS_REASON reason) { return TRUE; }

private:
	DialogContext*				m_dialog_context;
};

#endif //URL_FAV_MENU_BUTTON_H
