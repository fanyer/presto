/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef TEST_CONTROLLER_H
#define TEST_CONTROLLER_H

#ifdef SELFTEST

#include "adjunct/quick_toolkit/contexts/UiContext.h"
#include "adjunct/quick_toolkit/widgets/QuickWindow.h"

/**
 * A test context for displaying a QuickWidget in a QuickWindow.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class ST_TestController : public UiContext
{
public:
	ST_TestController() : m_window(NULL)  {}
	virtual ~ST_TestController()  { OP_DELETE(m_window); }

	OP_STATUS Show(QuickWidget* widget)
	{
		OpAutoPtr<QuickWidget> widget_holder(widget);
		OpAutoPtr<QuickWindow> window(OP_NEW(QuickWindow,
					(OpWindow::STYLE_MODAL_DIALOG, OpTypedObject::DIALOG_TYPE)));
		RETURN_OOM_IF_NULL(window.get());
		RETURN_IF_ERROR(window->Init());
		window->SetContent(widget_holder.release());
		window->SetContext(this);
		RETURN_IF_ERROR(window->Show());
		m_window = window.release();
		return OpStatus::OK;
	}

	QuickWindow* m_window;
};

#endif // SELFTEST

#endif // TEST_CONTROLLER_H
