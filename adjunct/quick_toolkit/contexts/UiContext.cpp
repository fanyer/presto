/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/contexts/UiContext.h"

UiContext::~UiContext()
{
	ReleaseFocus();
	g_main_message_handler->UnsetCallBacks(this);
}

void UiContext::AddChildContext(UiContext* child)
{
	child->SetParentInputContext(this);
	child->Into(&m_children);

	// Listen to messages from children
	g_main_message_handler->SetCallBack(this, MSG_QUICKTK_CLOSE_CONTEXT, reinterpret_cast<MH_PARAM_1>(child));
}

BOOL UiContext::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_LOWLEVEL_PREFILTER_ACTION:
			return CancelsAction(action->GetChildAction());
		case OpInputAction::ACTION_GET_ACTION_STATE:
			return OnActionGetState(action->GetChildAction());
	}

	BOOL handled = FALSE;

	if (CanHandleAction(action) && !DisablesAction(action))
		handled =  OpStatus::IsSuccess(HandleAction(action));

	return handled;
}

void UiContext::OnUiClosing()
{
	// Send message to close context to parent
	g_main_message_handler->PostMessage(MSG_QUICKTK_CLOSE_CONTEXT, reinterpret_cast<MH_PARAM_1>(this), 0);
}

BOOL UiContext::OnActionGetState(OpInputAction* action)
{
	if (CanHandleAction(action))
	{
		action->SetEnabled(!DisablesAction(action));
		action->SetSelected(SelectsAction(action));
		return TRUE;
	}

	return FALSE;
}

void UiContext::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	// Handle close message from children
	if (msg != MSG_QUICKTK_CLOSE_CONTEXT)
		return;

	g_main_message_handler->RemoveCallBacks(this, par1);

	UiContext* to_close = reinterpret_cast<UiContext*>(par1);

	OP_ASSERT(m_children.HasLink(to_close) || !"Deleting a context that's not owned by us");
	to_close->Out();
	OP_DELETE(to_close);
}
