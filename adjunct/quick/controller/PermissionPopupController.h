/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Cezary Kulakowski (ckulakowski)
 */

#ifndef PERMISSION_POPUP_CONTROLLER_H
#define PERMISSION_POPUP_CONTROLLER_H

#include "adjunct/quick_toolkit/contexts/OkCancelDialogContext.h"
#include "modules/windowcommander/OpWindowCommander.h"

class DocumentDesktopWindow;

class PermissionPopupController : public OkCancelDialogContext
{
public:
	PermissionPopupController(
			OpPermissionListener::PermissionCallback::PermissionType type,
			OpWidget& bounding_widget,
			OpWidget* anchor_widget,
			const uni_char* hostname,
			DocumentDesktopWindow* window)
	: m_type(type)
	, m_bounding_widget(bounding_widget)
	, m_anchor_widget(anchor_widget)
	, m_hostname(hostname)
	, m_desktop_window(window)
	, m_widgets(NULL)
	, m_deny_permission_on_closing(false)
	{}

	OP_STATUS SetTopLevelDomain(OpStringC toplevel_domain) { return m_toplevel_domain.Set(toplevel_domain); }
	void DenyPermissionOnClosing(bool deny) { m_deny_permission_on_closing = deny; }
	void SetIsOnBottom();

protected:
	// OkCancelDialogContext
	virtual void OnCancel();
	virtual void OnOk();

private:
	enum
	{
		ALWAYS_ALLOW,
		ALLOW_ONCE,
		DENY
	};

	virtual void InitL();
	virtual void OnKeyboardInputGained(OpInputContext* new_context, OpInputContext* old_context, FOCUS_REASON reason);

	OpPermissionListener::PermissionCallback::PermissionType m_type;
	OpWidget& m_bounding_widget;
	OpWidget* m_anchor_widget;
	const uni_char* m_hostname;
	OpString m_toplevel_domain;
	DocumentDesktopWindow* m_desktop_window;
	const TypedObjectCollection* m_widgets;
	bool m_deny_permission_on_closing;
};

#endif // PERMISSION_POPUP_CONTROLLER_H
