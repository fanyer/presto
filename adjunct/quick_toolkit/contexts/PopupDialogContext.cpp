/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/contexts/PopupDialogContext.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"


BOOL PopupDialogContext::ActionSelector::OnInputAction(OpInputAction* action)
{
	if (action->GetAction() == OpInputAction::ACTION_GET_ACTION_STATE)
	{
		action = action->GetChildAction();
		if (action->Equals(m_widget->GetAction()))
		{
			action->SetSelected(TRUE);
			return TRUE;
		}
	}
	else if (action->Equals(m_widget->GetAction()))
	{
		// So that a new pop-up cannot be opened before the current
		// one is closed.  We end up here when the user clicks the widget
		// multiple times very quickly.
		return TRUE;
	}
	return FALSE;
}


/**
 * A helper context whose sole purpose is swallowing the action associated with
 * a widget (once).
 *
 * This context is spawned from PopupDialogContext, but it outlives its
 * creator.  Hence, it takes care to self-destruct once it's swallowed the
 * action or it's determined there will be no action to swallow.
 */
class PopupDialogContext::ActionEater
	: public OpInputContext
	, public OpWidgetExternalListener
	, public DesktopWindowListener
{
public:
	static ActionEater* Create(OpWidget* widget, OpWidget* deletable_ancestor)
	{
		return OP_NEW(ActionEater, (widget, deletable_ancestor));
	}

	virtual ~ActionEater();

	// OpInputContext
	virtual BOOL OnInputAction(OpInputAction* action);

	// OpWidgetExternalListener
	virtual void OnDeleted(OpWidget *widget);

	// DesktopWindowListener
	virtual void OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated);

private:
	// Force creation by OP_NEW, so that it's safe to self-destruct.
	ActionEater(OpWidget* widget, OpWidget* deletable_ancestor);

	DesktopWindow* m_parent_desktop_window;
	OpWidget* m_widget;
	OpWidget* m_deletable_ancestor;
	OpInputContext* m_widget_parent_context;
};


PopupDialogContext::ActionEater::ActionEater(OpWidget* widget, OpWidget* deletable_ancestor)
	: m_parent_desktop_window(widget->GetParentDesktopWindow())
	, m_widget(widget)
	, m_deletable_ancestor(deletable_ancestor)
	, m_widget_parent_context(widget->GetParentInputContext())
{
	m_widget->SetParentInputContext(this, TRUE);
	SetParentInputContext(m_widget_parent_context, TRUE);

	m_parent_desktop_window->AddListener(this);
	g_opera->widgets_module.AddExternalListener(this);
}

PopupDialogContext::ActionEater::~ActionEater()
{
	g_opera->widgets_module.RemoveExternalListener(this);
	m_parent_desktop_window->RemoveListener(this);
	if (m_widget)
		m_widget->SetParentInputContext(m_widget_parent_context, TRUE);
}

BOOL PopupDialogContext::ActionEater::OnInputAction(OpInputAction* action)
{
	bool implode = false;

	if (action->Equals(m_widget->GetAction()))
		implode = true;
	else if (action->GetAction() == OpInputAction::ACTION_LOWLEVEL_PREFILTER_ACTION)
	{
		action = action->GetChildAction();
		if (action->GetAction() == OpInputAction::ACTION_LOWLEVEL_KEY_UP
				&& action->GetActionMethod() == OpInputAction::METHOD_MOUSE)
		{
			OpRect action_rect;
			action->GetActionPosition(action_rect);
			implode = m_widget->GetScreenRect().Contains(action_rect.TopLeft()) == FALSE;
		}
	}

	if (implode)
	{
		OP_DELETE(this);
		return TRUE;
	}

	return FALSE;
}

void PopupDialogContext::ActionEater::OnDeleted(OpWidget *widget)
{
	if (m_deletable_ancestor == widget)
	{
		SetParentInputContext(NULL, TRUE);
		m_deletable_ancestor = NULL;
		m_widget = NULL;
	}
}

void PopupDialogContext::ActionEater::OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated)
{
	OP_ASSERT(desktop_window == m_parent_desktop_window);
	OP_DELETE(this);
}


PopupDialogContext::PopupDialogContext(OpWidget* anchor_widget, OpWidget* deletable_ancestor)
	: m_anchor_widget(NULL)
	, m_deletable_ancestor(deletable_ancestor)
	, m_anchor_parent_context(NULL)
	, m_selector_context(NULL)
{
	SetAnchorWidget(anchor_widget);
	if (!m_deletable_ancestor)
		m_deletable_ancestor = m_anchor_widget;
}

PopupDialogContext::~PopupDialogContext()
{
	if (m_anchor_widget && m_anchor_widget->GetParentInputContext() == &m_selector_context)
		m_anchor_widget->SetParentInputContext(m_anchor_parent_context, TRUE);
}

void PopupDialogContext::OnUiCreated()
{
	g_application->SetPopupDesktopWindow(m_dialog->GetDesktopWindow());
}

void PopupDialogContext::OnUiClosing()
{
	if (m_anchor_widget)
	{
		m_selector_context.SetParentInputContext(NULL, TRUE);
		m_anchor_widget->SetParentInputContext(m_anchor_parent_context, TRUE);

		if (g_widget_globals->captured_widget == m_anchor_widget)
			// The dialog is closing because the mouse is down on the anchor
			// widget.  On mouse up, the widget will invoke the action to open the
			// dialog again.  Need to inject an ActionEater as the parent context
			// of the widget so that the action is not handled.
			ActionEater::Create(m_anchor_widget, m_deletable_ancestor);

		// Make the widget apply the "unselected" state.
		m_anchor_widget->SetUpdateNeeded(TRUE);
		m_anchor_widget->UpdateActionStateIfNeeded();

		// Lose the reference -- we're done with the anchor widget, and the
		// reference can go stale by the time we're destroyed.
		m_anchor_widget = NULL;
	}

	CloseDialogContext::OnUiClosing();
}

void PopupDialogContext::SetAnchorWidget(OpWidget* anchor_widget)
{
	// Inject an ActionSelector as the parent context of the anchor widget.
	// This way, the widget will get the "selected" state for as long as the
	// dialog is shown.

	if (m_anchor_widget && m_anchor_widget->GetParentInputContext() == &m_selector_context)
		m_anchor_widget->SetParentInputContext(m_anchor_parent_context, TRUE);

	m_anchor_widget = anchor_widget;
	m_selector_context.SetWidget(m_anchor_widget);
	m_anchor_parent_context = m_anchor_widget ? m_anchor_widget->GetParentInputContext() : NULL;

	if (m_anchor_widget)
		m_anchor_widget->SetParentInputContext(&m_selector_context, TRUE);
	m_selector_context.SetParentInputContext(m_anchor_parent_context, TRUE);
}
