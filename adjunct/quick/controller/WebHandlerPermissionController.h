/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef WEB_HANDLER_PERMISSION_CONTROLLER_H
#define WEB_HANDLER_PERMISSION_CONTROLLER_H

#include "adjunct/desktop_util/adt/opproperty.h"
#include "adjunct/quick_toolkit/contexts/OkCancelDialogContext.h"
#include "modules/windowcommander/OpWindowCommander.h"

/**
 * Controller for the registered web handlers usage permission approval
 */
class WebHandlerPermissionController : public OkCancelDialogContext
{
public:
	/**
	 * Create a new dialog associcated with the callback and window
	 */
	static void Add(OpDocumentListener::WebHandlerCallback* cb, DesktopWindow* parent);

	/**
	 * Close and destroy the dialog without touching the callback object.
	 * Typically to be used when core requests a cancellation the dialog
	 */
	static void Remove(OpDocumentListener::WebHandlerCallback* cb);

	~WebHandlerPermissionController();

	virtual OP_STATUS HandleAction(OpInputAction* action);
	virtual BOOL DisablesAction(OpInputAction* action);
	OP_STATUS GetApplication(OpString& application, BOOL validate);

	virtual void OnTextChanged(const OpStringC& text);

protected:
	virtual void OnCancel();
	virtual void OnOk();

private:
	explicit WebHandlerPermissionController(OpDocumentListener::WebHandlerCallback* cb)
		: m_webhandler_cb(cb)
		, m_widgets(0)
		, m_mode(NotDefined)
		, m_is_mailto(false)
		{}

	// DialogContext
	virtual void InitL();
	virtual void OnRadioChanged(bool new_value);

	enum Mode
	{
		NotDefined = -1,
		// These types are used in array indexing. See m_radio_value
		WebHandler = 0,
		DefaultApplication = 1,
		CustomApplication = 2,
		MaxModeCount = 3 // Always last
	};

	OpDocumentListener::WebHandlerCallback* m_webhandler_cb;
	const TypedObjectCollection* m_widgets;
	Mode m_mode;
	OpString m_application;
	OpProperty<OpString> m_application_text;
	OpProperty<bool> m_radio_value[3];
	bool m_is_mailto;
	OpProperty<bool> m_dont_ask_again;
	static OpVector<WebHandlerPermissionController> m_list;
};

#endif // WEB_HANDLER_PERMISSION_CONTROLLER_H
