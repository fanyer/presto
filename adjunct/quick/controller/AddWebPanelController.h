/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef ADD_WEB_PANEL_CONTROLLER_H
#define ADD_WEB_PANEL_CONTROLLER_H

#include "adjunct/quick_toolkit/contexts/OkCancelDialogContext.h"

class HotlistModelItem;
class BookmarkItemData;

class AddWebPanelController
		: public OkCancelDialogContext
{
public:
	// OkCancelDialogContext
	virtual OP_STATUS		HandleAction(OpInputAction* action);

	// UiContext
	virtual BOOL			CanHandleAction(OpInputAction* action);

private:
	// OkCancelDialogContext
	virtual void			InitL();

	void					InitAddressL();
	void					SetupGetWebPabelLabelL();
	void					ShowPanel(HotlistModelItem &hotlist_item, BookmarkItemData &bookmark_item);
	HotlistModelItem*		GetHotlistItem(const uni_char *url_str);
	const uni_char*			GetCurrentURL();
};

#endif //ADD_WEB_PANEL_CONTROLLER_H
