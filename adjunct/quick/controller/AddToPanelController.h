/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef ADD_TO_PANEL_CONTROLLER_H
#define ADD_TO_PANEL_CONTROLLER_H

#include "adjunct/quick_toolkit/contexts/OkCancelDialogContext.h"
#include "adjunct/quick/models/BookmarkModel.h"

class AddToPanelController
		: public OkCancelDialogContext
{
public:
	AddToPanelController();

public:
	OP_STATUS				InitPanel(const uni_char *title, const uni_char *url, bool &show_dialog);

protected:
	// OkCancelDialogContext
	virtual void			OnOk();

private:
	// OkCancelDialogContext
	virtual void			InitL();

	HotlistModelItem*		GetHotlistItem(const uni_char *url_str);
	void					ShowPanel(HotlistModelItem &hotlist_item, BookmarkItemData &bookmark_item);

private:
	BookmarkItemData		m_bookmark_item_data;
	HotlistModelItem		*m_hotlist_item;
};

#endif //ADD_TO_PANEL_CONTROLLER_H
