/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/display/vis_dev.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/win.h"
#include "modules/dochand/windowlistener.h"
#include "modules/dochand/winman.h"
#include "modules/pi/OpView.h"
#ifdef GADGET_SUPPORT
#include "modules/gadgets/OpGadget.h"
#endif // GADGET_SUPPORT

#ifdef SCOPE_WINDOW_MANAGER_SUPPORT
# include "modules/scope/scope_window_listener.h"
#endif // SCOPE_WINDOW_MANAGER_SUPPORT

WindowListener::WindowListener(Window *window)
	: window(window)
{
}

void WindowListener::OnResize(UINT32 width, UINT32 height)
{
#ifdef NEARBY_ELEMENT_DETECTION
	window->SetElementExpander(NULL);
#endif // NEARBY_ELEMENT_DETECTION

	window->HandleNewWindowSize(width, height);
}

void WindowListener::OnMove()
{
#ifdef NEARBY_ELEMENT_DETECTION
	window->SetElementExpander(NULL);
#endif // NEARBY_ELEMENT_DETECTION

#ifdef _MACINTOSH_
	window->VisualDev()->UpdateScrollbars();
#endif
}

void WindowListener::OnRenderingBufferSizeChanged(UINT32 width, UINT32 height)
{
	if (width == 0 || height == 0)
		return;

	OpRect rendering_rect(0, 0, width, height);

#ifdef _PRINT_SUPPORT_
	if (window->GetPreviewMode())
		if (VisualDevice* print_preview_vis_dev = window->DocManager()->GetPrintPreviewVD())
			print_preview_vis_dev->SetRenderingViewGeometryScreenCoords(rendering_rect);
#endif // _PRINT_SUPPORT_

	if (VisualDevice* normal_vis_dev = window->VisualDev())
		normal_vis_dev->SetRenderingViewGeometryScreenCoords(rendering_rect);
}

void WindowListener::OnActivate(BOOL active)
{
#ifdef NEARBY_ELEMENT_DETECTION
	window->SetElementExpander(NULL);
#endif // NEARBY_ELEMENT_DETECTION

	if (active)
	{
		FramesDocument * doc = window->GetActiveFrameDoc();
		VisualDevice *visdev;
		if (doc)
			visdev = doc->GetVisualDevice();
		else
			visdev = window->VisualDev();

		visdev->RestoreFocus(FOCUS_REASON_ACTIVATE);

#ifdef DOM_JIL_API_SUPPORT
		if (window->GetType() == WIN_TYPE_GADGET && window->GetGadget())
			OpStatus::Ignore(window->GetGadget()->OnFocus());
#endif // DOM_JIL_API_SUPPORT

#ifdef SCOPE_WINDOW_MANAGER_SUPPORT
		OpScopeWindowListener::OnActiveWindowChanged(window);
#endif // SCOPE_WINDOW_MANAGER_SUPPORT
	}
	else
	{
		window->VisualDev()->ReleaseFocus();
	}

	DocumentTreeIterator iterator(window);
	iterator.SetIncludeThis();
	while (iterator.Next())
	{
		FramesDocument* cur_doc = iterator.GetFramesDocument();
		OP_ASSERT(cur_doc);
		DOM_Environment* env = cur_doc->GetDOMEnvironment();
		if (env)
			if (active)
				env->OnWindowActivated();
			else
				env->OnWindowDeactivated();
	}
}

#ifdef GADGET_SUPPORT
void WindowListener::OnShow(BOOL show)
{
	if (window->GetType() == WIN_TYPE_GADGET)
	{
		if (show)
			window->GetGadget()->OnShow();
		else
			window->GetGadget()->OnHide();
	}
}
#endif // GADGET_SUPPORT

void WindowListener::OnVisibilityChanged(BOOL vis)
{
#ifdef NEARBY_ELEMENT_DETECTION
	window->SetElementExpander(NULL);
#endif // NEARBY_ELEMENT_DETECTION

	FramesDocument * doc = window->GetActiveFrameDoc();
	if (doc)
	{
		OpRect rendering_viewport = doc->GetVisualDevice()->GetRenderingViewport();

		if (!vis)
			rendering_viewport.width = rendering_viewport.height = 0;

		doc->OnRenderingViewportChanged(rendering_viewport);
	}
	window->SetVisibleOnScreen(vis);

	DocumentTreeIterator iterator(window);
	iterator.SetIncludeThis();
	while (iterator.Next())
	{
		FramesDocument* cur_doc = iterator.GetFramesDocument();
		cur_doc->OnVisibilityChanged();
		if (VisualDevice* visdev = cur_doc->GetVisualDevice())
			visdev->OnVisibilityChanged(vis);
	}
}

