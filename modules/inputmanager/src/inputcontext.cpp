// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
//
// Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

#include "core/pch.h"

#include "modules/inputmanager/inputcontext.h"

/***********************************************************************************
**
**	OpInputContext
**
***********************************************************************************/

OpInputContext::~OpInputContext()
{
	if (g_input_manager)
	{
		g_input_manager->ReleaseInputContext(this);
	}
}

#ifdef WIDGETS_IME_SUPPORT

IM_WIDGETINFO OpInputContext::OnStartComposing(OpInputMethodString* /*imstring*/, IM_COMPOSE /*compose*/)
{
	return GetWidgetInfo();
}

IM_WIDGETINFO OpInputContext::OnCompose()
{
	return GetWidgetInfo();
}

IM_WIDGETINFO OpInputContext::GetWidgetInfo()
{
	IM_WIDGETINFO widgetinfo;
	op_memset(&widgetinfo, 0, sizeof(widgetinfo));
	return widgetinfo;
}

#endif

/***********************************************************************************
**
**	OnKeyboardInputGained
**
***********************************************************************************/

void OpInputContext::OnKeyboardInputGained(OpInputContext* new_input_context, OpInputContext* /*old_input_context*/, FOCUS_REASON reason)
{
	if (new_input_context == this)
	{
		OnFocus(TRUE, reason);
	}
}

/***********************************************************************************
**
**	OnKeyboardInputLost
**
***********************************************************************************/

void OpInputContext::OnKeyboardInputLost(OpInputContext* /*new_input_context*/, OpInputContext* old_input_context, FOCUS_REASON reason)
{
	if (old_input_context == this)
	{
		OnFocus(FALSE, reason);
	}
}

/***********************************************************************************
**
**	SetParentInputContext
**
***********************************************************************************/

void OpInputContext::SetParentInputContext(OpInputContext* parent_context, BOOL keep_keyboard_focus /*= FALSE*/)
{
	if (g_input_manager)
	{
		g_input_manager->ReleaseInputContext(this, FOCUS_REASON_RELEASE, keep_keyboard_focus);
	}

	m_parent_context = parent_context;
}

/***********************************************************************************
**
**	IsParentInputContextOf
**
***********************************************************************************/

BOOL OpInputContext::IsParentInputContextOf(OpInputContext* child_context)
{
	if (child_context == this)
		return FALSE; //You are not your own parent

	while (child_context && child_context != this)
	{
		child_context = child_context->GetParentInputContext();
	}
	return child_context != NULL;
}

/***********************************************************************************
**
**	IsChildInputContextOf
**
***********************************************************************************/

BOOL OpInputContext::IsChildInputContextOf(OpInputContext* parent_context)
{
	if (!parent_context)
		return FALSE;

	return parent_context->IsParentInputContextOf(this);
}

/***********************************************************************************
**
**	IsFocused
**
***********************************************************************************/

BOOL OpInputContext::IsFocused(BOOL HasFocusedChildren /* = FALSE */)
{
	OpInputContext* focused = g_input_manager->GetKeyboardInputContext();

	if (HasFocusedChildren)
		return IsParentInputContextOf(focused);
	else
		return focused == this;
}
