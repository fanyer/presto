/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "x11_vegawindow.h"
#include "platforms/unix/base/x11/x11_mdescreen.h"

void X11VegaWindow::OnPresentComplete()
{
	m_is_present_in_progress = false;
	X11MdeScreen * screen = getMdeScreen();
	if (screen)
		screen->OnPresentComplete();
}
