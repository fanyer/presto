/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef CUSTOM_WEB_FEED_CONTROLLER_H
#define CUSTOM_WEB_FEED_CONTROLLER_H

#include "adjunct/desktop_util/widget_utils/VisualDeviceHandler.h"
#include "adjunct/quick_toolkit/contexts/OkCancelDialogContext.h"

/**
 * Controller managing custom web feed engines
 */
class CustomWebFeedController : public OkCancelDialogContext
{
private:
	struct Item
	{
	public:
		Item(INT32 id):m_id(id),m_deleted(FALSE) {}

		INT32 m_id;
		BOOL m_deleted;
		OpString m_name;
	};

public:
	CustomWebFeedController()
		:m_widgets(0), m_visdev_handler(0) {}
	~CustomWebFeedController() { OP_DELETE(m_visdev_handler); }

	virtual BOOL DisablesAction(OpInputAction* action);
	virtual BOOL CanHandleAction(OpInputAction* action);
	virtual OP_STATUS HandleAction(OpInputAction* action);

protected:
	virtual void OnOk();

private:
	// DialogContext
	virtual void InitL();

private:
	const TypedObjectCollection* m_widgets;
	VisualDeviceHandler* m_visdev_handler;
	OpAutoVector<Item> m_list;

};

#endif // CUSTOM_WEB_FEED_CONTROLLER_H
