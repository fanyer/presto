/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Bazyli Zygan bazyl@opera.com
 */

#ifndef GADGET_PREFERENCES_DIALOG_H
#define GADGET_PREFERENCES_DIALOG_H

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/quick_toolkit/widgets/Dialog.h"


class GadgetPreferencesDialog
: public Dialog
{
public:
	enum PermissionType
	{
		ALLOW,
		DISALLOW,
		ASK
	};

	GadgetPreferencesDialog(PermissionType type) : m_permission_type(type) {}
	
	/**
	 * Using DIALOG_TYPE_WIDGET_INSTALLER, we don't need new one.
	 */
	DialogType GetDialogType() { return TYPE_OK_CANCEL; }
	virtual const char* GetWindowName() { return "Gadget Preferences Dialog"; }
	
	/**
	 * Dialog initialization.
	 *
	 * @param src_wgt_path Path to widget wgt file.
	 * @return Error status.
	 */
	OP_STATUS Init();
	
	/**
	 * Closing the dialog is terminating, if the dialog was started without
	 * the browser.
	 */
	virtual UINT32 OnOk();
	
	// == OpWidgetListener ======================
	
	//virtual void OnChange(OpWidget *widget, BOOL changed_by_mouse);
	virtual BOOL OnInputAction(OpInputAction* action);
private:
#ifdef DOM_GEOLOCATION_SUPPORT
	OpString				m_gadget_identifier;
	PermissionType			m_permission_type;
#endif //DOM_GEOLOCATION_SUPPORT
};

#endif // WIDGET_RUNTIME_SUPPORT
#endif // GADGET_PREFERENCES_DIALOG_H

