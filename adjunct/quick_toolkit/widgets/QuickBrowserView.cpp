/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/QuickBrowserView.h"

#include "adjunct/desktop_pi/DesktopOpWindow.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"


DEFINE_CONSTRUCT(BrowserView);

OP_STATUS BrowserView::Init()
{
	RETURN_IF_ERROR(OpBrowserView::Init());

	if (GetParentDesktopWindow() != NULL)
		RETURN_IF_ERROR(GetParentDesktopWindow()->AddListener(this));

	return OpStatus::OK;
}

void BrowserView::OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated)
{
	OP_ASSERT(desktop_window == GetParentDesktopWindow() || GetParentDesktopWindow() == NULL);
	OpBrowserView::OnDeleted();
}

void BrowserView::OnAdded()
{
	OpBrowserView::OnAdded();
	GetOpWindow()->Show(TRUE);
	if (m_listener != NULL)
		m_listener->OnBrowserViewReady(*this);
}

void BrowserView::OnRemoved()
{
	OpBrowserView::OnRemoved();
	GetOpWindow()->Show(FALSE);
}

void BrowserView::OnDeleted()
{
	OpBrowserView::OnDeleted();
	if (GetParentDesktopWindow() != NULL)
		GetParentDesktopWindow()->RemoveListener(this);
}
