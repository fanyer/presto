/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/desktop_pi/DesktopOpView.h"
#include "platforms/mac/pi/MacOpView.h"
#include "platforms/mac/util/macutils.h"
#include "modules/libgogi/pi_impl/mde_opwindow.h"
#include "modules/libgogi/pi_impl/mde_opview.h"
#include "platforms/mac/pi/CocoaOpWindow.h"
#ifdef NO_CARBON
#include "platforms/mac/quick_support/CocoaQuickSupport.h"
#include "platforms/mac/util/MachOCompatibility.h"
#endif

OP_STATUS DesktopOpView::Create(OpView **new_obj, OpWindow *parentwindow)
{
	*new_obj = OP_NEW(MacOpView, ());
	if (!*new_obj)
		return OpStatus::ERR_NO_MEMORY;
	OP_STATUS result = ((MacOpView *)*new_obj)->Init(parentwindow);
	if (OpStatus::IsError(result))
	{
		OP_DELETE(*new_obj);
		*new_obj = 0;
	};
	return result;
}

OP_STATUS DesktopOpView::Create(OpView **new_obj, OpView *parentview)
{
	*new_obj = OP_NEW(MacOpView, ());
	if (!*new_obj)
		return OpStatus::ERR_NO_MEMORY;
	OP_STATUS result = ((MacOpView *)*new_obj)->Init(parentview);
	if (OpStatus::IsError(result))
	{
		OP_DELETE(*new_obj);
		*new_obj = 0;
	};
	return result;
}

void MacOpView::GetMousePos(INT32 *xpos, INT32 *ypos)
{
	Point pt;
	GetGlobalMouse(&pt);
	OpPoint point;
	point = ConvertToScreen(point);
	*xpos = pt.h-point.x;
	*ypos = pt.v-point.y;
}

OpPoint MacOpView::ConvertToScreen(const OpPoint &point)
{
	return ::ConvertToScreen(point, NULL, (MDE_OpView*)this);
}

OpPoint MacOpView::ConvertFromScreen(const OpPoint &point)
{
	OpPoint pt = ::ConvertToScreen(OpPoint(), NULL, (MDE_OpView*)this);
	return MDE_OpView::ConvertFromScreen(point - pt);
}

OpPoint MacOpView::MakeGlobal(const OpPoint &point, BOOL stop_at_toplevel)
{
	// Later: Move the loop from ConvertToScreen here, and make that function call us, to avoid adding and subtracting the same value
	OpPoint pt = ConvertToScreen(point);
	if (stop_at_toplevel) {
		OpWindow* root = GetRootWindow();
		INT32 xpos,ypos;
		root->GetInnerPos(&xpos, &ypos);
		pt.x -= xpos;
		pt.y -= ypos;
	}
	return pt;
}

OpWindow* MacOpView::GetRootWindow()
{
	if (GetParentView())
		return GetParentView()->GetRootWindow();
	else if (GetParentWindow())
		return GetParentWindow()->GetRootWindow();
	return NULL;
}

void MacOpView::SetDragListener(OpDragListener* inDragListener)
{
	mDragListener = inDragListener;
	MDE_OpView::SetDragListener(inDragListener);
}

void MacOpView::RemoveDelayedInvalidate(DelayedInvalidation* action)
{
	mDelayedActions.RemoveByItem(action);
}

void MacOpView::DelayedInvalidate(OpRect rect, UINT32 timeout)
{
	for(UINT32 i=0; i<mDelayedActions.GetCount(); i++)
	{
		DelayedInvalidation *inv = mDelayedActions.Get(i);
		if(inv && inv->m_view == this && inv->m_rect.Equals(rect))
		{
			return;
		}
	}
	mDelayedActions.Add(new DelayedInvalidation(this, rect, timeout));
}

#ifdef WIDGETS_IME_SUPPORT
void MacOpView::AbortInputMethodComposing()
{
	CocoaOpWindow* win = static_cast<CocoaOpWindow*>(GetRootWindow());
	if (win) {
		win->AbortInputMethodComposing();
	}
}

void MacOpView::SetInputMethodMode(IME_MODE mode, IME_CONTEXT context, const uni_char* istyle)
{
	static TISInputSourceRef lastInput = NULL;
	if (mode == IME_MODE_PASSWORD) {
		if (TISCopyCurrentKeyboardInputSource && TISSetInputMethodKeyboardLayoutOverride) {
			if (!lastInput) {
				lastInput = TISCopyCurrentKeyboardInputSource();
			}
			TISSelectInputSource(TISCopyCurrentASCIICapableKeyboardLayoutInputSource());
		}
		EnableSecureEventInput();
	}
	else {
		DisableSecureEventInput();
		if (lastInput && TISSetInputMethodKeyboardLayoutOverride) {
			TISSelectInputSource(lastInput);
			lastInput = NULL;
		}
	}
}
#endif
