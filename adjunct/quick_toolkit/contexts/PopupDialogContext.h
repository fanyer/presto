/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef POPUP_DIALOG_CONTEXT_H
#define POPUP_DIALOG_CONTEXT_H

#include "adjunct/quick_toolkit/contexts/CloseDialogContext.h"

/**
 * A popup dialog context.
 *
 * It is meant to be used with dialogs that should behave like
 * OpWindow::STYLE_POPUP windows.  It manages the action state to reflect the
 * open/closed state of the dialog.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class PopupDialogContext : public CloseDialogContext
{
public:
	/**
	 * @param deletable_ancestor an ancestor of @a anchor_widget that is used
	 * as the source of information on when the anchor is deleted. The purpose
	 * is to make sure we're not accessing the anchor widget once it's gone.
	 * If @c NULL, it is assumed that @a deletable_ancestor is @a anchor_widget.
	 */
	explicit PopupDialogContext(OpWidget* anchor_widget, OpWidget* deletable_ancestor = NULL);
	virtual ~PopupDialogContext();

	void SetAnchorWidget(OpWidget* anchor_widget);

	// UiContext
	virtual void OnUiCreated();
	virtual void OnUiClosing();
	
protected:
	OpWidget* GetAnchorWidget() const { return m_anchor_widget; }

private:
	/**
	 * A helper context whose sole purpose is setting the "selected" state of
	 * the action associated with a widget.
	 */
	class ActionSelector : public OpInputContext
	{
	public:
		explicit ActionSelector(OpWidget* widget) : m_widget(widget) {}
		virtual BOOL OnInputAction(OpInputAction* action);
		void SetWidget(OpWidget* widget) { m_widget = widget; }
	private:
		OpWidget* m_widget;
	};

	class ActionEater;

	OpWidget* m_anchor_widget;
	OpWidget* m_deletable_ancestor;
	OpInputContext* m_anchor_parent_context;
	ActionSelector m_selector_context;
};

#endif // POPUP_DIALOG_CONTEXT_H
