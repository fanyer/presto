/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/desktop_pi/DesktopOpView.h"

#include "modules/pi/OpView.h"
#ifdef VEGA_OPPAINTER_SUPPORT
# include "modules/libgogi/pi_impl/mde_opview.h"
#endif // VEGA_OPPAINTER_SUPPORT

OP_STATUS OpView::Create(OpView **new_opview, OpWindow *parentwindow)
{
	OP_ASSERT(new_opview);

#ifdef VEGA_OPPAINTER_SUPPORT
	if (parentwindow &&
		parentwindow->GetStyle() == OpWindow::STYLE_BITMAP_WINDOW)
	{
		MDE_OpView* opview = OP_NEW(MDE_OpView, ());
		RETURN_OOM_IF_NULL(opview);

		OP_STATUS status = opview->Init(parentwindow);
		if (OpStatus::IsError(status))
		{
			OP_DELETE(opview);
			*new_opview = NULL;
			return status;	
		}
		else
		{
			*new_opview = opview;
			return OpStatus::OK;	
		}
	}
#endif // VEGA_OPPAINTER_SUPPORT

	return DesktopOpView::Create(new_opview, parentwindow);
}

OP_STATUS OpView::Create(OpView **new_opview, OpView *parentview)
{
	OP_ASSERT(new_opview);

#ifdef VEGA_OPPAINTER_SUPPORT
	OpWindow *root_window = parentview ? parentview->GetRootWindow() : NULL;

	if (root_window &&
		root_window->GetStyle() == OpWindow::STYLE_BITMAP_WINDOW)
	{
		MDE_OpView *opview = OP_NEW(MDE_OpView, ());
		RETURN_OOM_IF_NULL(opview);

		OP_STATUS status = opview->Init(parentview);
		if (OpStatus::IsError(status))
		{
			OP_DELETE(opview);
			*new_opview = NULL;
			return status;	
		}
		else
		{
			*new_opview = opview;
			return OpStatus::OK;	
		}	
	}
#endif // VEGA_OPPAINTER_SUPPORT

	return DesktopOpView::Create(new_opview, parentview);
}


