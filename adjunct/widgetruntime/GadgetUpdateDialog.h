/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef GADGET_UPDATE_DIALOG
#define GADGET_UPDATE_DIALOG

#ifdef WIDGETS_UPDATE_SUPPORT

#include "adjunct/quick/dialogs/Dialog.h"
#include "adjunct/autoupdate_gadgets/GadgetUpdateController.h"

#include "modules/windowcommander/OpWindowCommanderManager.h"


class GadgetUpdateDialog : public Dialog, public GadgetUpdateListener
{
public:

	GadgetUpdateDialog();
	/**
	* initializes dialog
	* @param controller - logic which controls gadget update
	* @param upd_descr - description of particular update, change log
	* @param gadget - used for obtaining name, author 
	* @param confirmed - when true, dialog will display 1st page waiting 
	*					for user confirmation to update. When false, 
	*					update starts just after dialog is displayed
	*/
	virtual OP_STATUS Init(WidgetUpdater& updater,const OpStringC& upd_descr,OpGadgetClass* gadget,BOOL confirmed);

	virtual Type GetType(){ return DIALOG_TYPE_WIDGET_INSTALLER; }

	virtual INT32 GetButtonCount() { return BUTTON_COUNT; }

	virtual const char* GetWindowName() { return "Gadget Update Dialog"; }

	virtual void OnInitVisibility();
	virtual BOOL OnInputAction(OpInputAction *action);

	virtual void GetButtonInfo(INT32 index, OpInputAction*& action,
		OpString& text, BOOL& is_enabled, BOOL& is_default);

	
	//======================= GadgetUpdateListener ================================
	void OnGadgetUpdateFinish(GadgetUpdateInfo* data,
		GadgetUpdateController::GadgetUpdateResult result);

private:

	void UpdateConfimed();

	OpString m_update_details;
	OpString m_widget_name;
	OpString m_widget_author;
	WidgetUpdater* m_updater;

	static const INT32 BUTTON_COUNT = 3;

};

#endif //WIDGETS_UPDATE_SUPPORT
#endif //GADGET_UPDATE_DIALOG