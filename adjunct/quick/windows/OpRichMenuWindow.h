/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_RICH_MENU_WINDOW_H
#define OP_RICH_MENU_WINDOW_H

#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/quick_toolkit/widgets/OpToolbar.h"

class OpWidget;

class OpRichMenuToolbar : public OpToolbar
{
public:
	static OP_STATUS Construct(OpRichMenuToolbar** toolbar);

	// == OpInputContext ======================
	BOOL					IsInputContextAvailable(FOCUS_REASON reason) { return TRUE; }
};

/** Popup menu using a vertical toolbar for its contents. */
class OpRichMenuWindow : public DesktopWindow
{
public :
	class Listener
	{
	public:
		virtual ~Listener() {}
		virtual void OnMenuClosed() {};
	};

	/** Widget will animate if the style is set to OpWindow::STYLE_NOTIFIER. */
	OP_STATUS Construct(const char *toolbar_name, OpWindow::Style style = OpWindow::STYLE_POPUP);

	OpRichMenuWindow(OpWidget *owner, DesktopWindow* parent_window);
	virtual ~OpRichMenuWindow();

	/** Show menu and call back to the given listener. */
	void Show(Listener &listener);

	/** Close menu. Will call listeners OnMenuClosed.
		Use this method instead of DesktopWindow::Close! */
	void CloseMenu();

	OpWidget* GetToolbarWidgetOfType(OpTypedObject::Type type);

	// DesktopWindow methods
	virtual void			OnLayout();
	virtual void			OnShow(BOOL show);
	virtual void			OnClose(BOOL user_initiated);
	virtual void			Close(BOOL immediately = FALSE, BOOL user_initiated = FALSE, BOOL force = TRUE);

	virtual const char*		GetWindowName() { return "Rich Menu Window"; }

	// OpInputContext
	virtual BOOL			IsInputContextAvailable(FOCUS_REASON reason) { return TRUE; }
	virtual const char*		GetInputContextName() {return "Rich Menu Window";}

private:
	void Animate(BOOL show);

private:
	BOOL m_close_on_complete;
	BOOL m_animate;
	OpRichMenuToolbar *m_toolbar;
	OpWidget *m_owner;
	Listener *m_listener;
	DesktopWindow* m_parent_window;
};


#endif // OP_RICH_MENU_WINDOW_H
