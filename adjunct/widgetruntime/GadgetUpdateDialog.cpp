/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef WIDGETS_UPDATE_SUPPORT
#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/widgetruntime/GadgetUpdateDialog.h"
#include "adjunct/autoupdate_gadgets/GadgetUpdateController.h"
#include "adjunct/quick_toolkit/widgets/OpIcon.h"

#include "modules/widgets/OpButton.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/widgets/OpWidget.h"
#include "modules/gadgets/OpGadgetClass.h"


	const char LABEL_UPDATE_INFO[] = "label_update_info";
	const char IMPORT_WELCOME_LABEL[] = "Import_welcome_label";
	const char LABEL_WGT_NAME[] =  "label_wgt_name";
	const char LABEL_WGT_NAME2[] =  "label_wgt_name2";
	const char LABEL_WGT_NAME3[] =  "label_wgt_name3";
	const char LABEL_WGT_AUTHOR[] = "label_wgt_author";
	const char LABEL_WGT_AUTHOR2[] = "label_wgt_author2";
	const char LABEL_WGT_AUTHOR3[] = "label_wgt_author3";
	const char LABEL_UPDATE_RESULT[] = "label_update_result";

	enum InstallerButton
	{
		BUTTON_UPDATE = 0,
		BUTTON_CANCEL,
		BUTTON_LATER
		
	};

	enum PageNumber
	{
		PAGE_MAIN = 0,
		PAGE_PROGRESS,
		PAGE_FINAL
	};

#ifdef _MACINTOSH_
	// Button order is reversed on Mac.
	const INT32 BUTTON_FINISH = BUTTON_CANCEL;
#else
	const INT32 BUTTON_FINISH = BUTTON_LATER;
#endif



GadgetUpdateDialog::GadgetUpdateDialog(): m_updater(NULL)
{
}

OP_STATUS GadgetUpdateDialog::Init(WidgetUpdater &updater,
								   const OpStringC& upd_descr,
								   OpGadgetClass* gadget,
								   BOOL  confirmed)
{
	RETURN_IF_ERROR(m_update_details.Set(upd_descr));
	m_updater = &updater;

	if (gadget)
	{
		RETURN_IF_ERROR(gadget->GetGadgetName(m_widget_name));
		RETURN_IF_ERROR(gadget->GetGadgetAuthor(m_widget_author));
	}

	// since top level desktop window will be closed in case of update (gadget desktop window in WR), 
	// we can't make this dialog modal
	RETURN_IF_ERROR(Dialog::Init(NULL));
	
	if (confirmed)
	{
		UpdateConfimed();
	}
	return OpStatus::OK;
}

void GadgetUpdateDialog::OnInitVisibility()
{
	SetWidgetText(LABEL_UPDATE_INFO,m_update_details.CStr());
	SetWidgetText(LABEL_WGT_NAME,m_widget_name.CStr());
	SetWidgetText(LABEL_WGT_AUTHOR,m_widget_author.CStr());

	SetLabelInBold(LABEL_WGT_NAME);
	SetLabelInBold(LABEL_WGT_NAME2);
	SetLabelInBold(LABEL_WGT_NAME3);

}

void	GadgetUpdateDialog::GetButtonInfo(INT32 index,
			  OpInputAction*& action,	OpString& text, BOOL& is_enabled,
			  BOOL& is_default)
{
	is_enabled = TRUE;
	is_default = FALSE;
	

	switch (index)
	{
		case BUTTON_UPDATE:
			action = OP_NEW(OpInputAction,(OpInputAction::ACTION_CONFIRM_GADGET_UPDATE));
			g_languageManager->GetString(Str::D_MAIL_INDEX_PROPERTIES_UPDATE,text);
			is_default = TRUE;
			break;

		case BUTTON_LATER:
			action = OP_NEW(OpInputAction,(OpInputAction::ACTION_UPDATE_GADGET_DELAY));
			g_languageManager->GetString(Str::D_WAND_NOTNOW, text);
			break;

		case BUTTON_CANCEL:
			action = GetCancelAction();
			g_languageManager->GetString(Str::D_WIDGET_UPDATE_DIALOG_DONT, text); 
			break;

		default:
			OP_ASSERT(!"Unexpected button index");
	}
}


BOOL GadgetUpdateDialog::OnInputAction(OpInputAction *action)
{
	OP_ASSERT(NULL != action);
	if (NULL == action)
	{
		return FALSE;
	}

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_CONFIRM_GADGET_UPDATE:
		{
			UpdateConfimed();
			return TRUE;
		}

		case OpInputAction::ACTION_UPDATE_GADGET_DELAY:
		{
			m_updater->DelayGadgetUpdate();
			Close();
			return TRUE;
		}
		case OpInputAction::ACTION_CANCEL:
		{
			m_updater->CancelGadgetUpdate();
			Close();
			return TRUE;
		}
	}
	return Dialog::OnInputAction(action);
}

void GadgetUpdateDialog::UpdateConfimed()
{
	ShowButton(BUTTON_UPDATE, FALSE);
	ShowButton(BUTTON_CANCEL, FALSE);

	OpString finish_text;
	g_languageManager->GetString(Str::S_WIZARD_FINISH, finish_text);
	SetButtonText(BUTTON_FINISH, finish_text);
	EnableButton(BUTTON_FINISH, FALSE);

	SetCurrentPage(PAGE_PROGRESS);
	OpIcon* progress_icon = static_cast<OpIcon*>(
		GetWidgetByName("gadget_update_status_icon"));

	OP_ASSERT(NULL != progress_icon);

	progress_icon->SetImage("Thumbnail Busy Image"); 

	SetWidgetText(LABEL_WGT_NAME2,m_widget_name.CStr());
	SetWidgetText(LABEL_WGT_AUTHOR2,m_widget_author.CStr());
	
	m_updater->ConfirmGadgetUpdate();
}

void GadgetUpdateDialog::OnGadgetUpdateFinish(GadgetUpdateInfo* data,
	GadgetUpdateController::GadgetUpdateResult result)
{
	// as soon as finish status is displayed
	// we no longer need to listen for feature update statuses
	m_updater->GetController()->RemoveUpdateListener(this);
	SetWidgetText(LABEL_WGT_NAME3,m_widget_name.CStr());
	SetWidgetText(LABEL_WGT_AUTHOR3,m_widget_author.CStr());

	SetCurrentPage(PAGE_FINAL);
	SetButtonAction(BUTTON_FINISH, GetOkAction());

	SetButtonAction(BUTTON_FINISH, GetOkAction());

	FocusButton(BUTTON_FINISH);
	EnableButton(BUTTON_FINISH, TRUE);

	switch(result)
	{
		case GadgetUpdateController::UPD_FATAL_ERROR:
			SetWidgetText(LABEL_UPDATE_RESULT,Str::D_WIDGET_UPDATE_FATAL_ERROR);
			break;

		case GadgetUpdateController::UPD_FAILED:
			SetWidgetText(LABEL_UPDATE_RESULT,Str::D_WIDGET_UPDATE_FAIL);
			break;

		case GadgetUpdateController::UPD_SUCCEDED:
			SetWidgetText(LABEL_UPDATE_RESULT,Str::D_WIDGET_UPDATE_SUCCESS);
			break;

		default:
			OP_ASSERT(!"Unexpected index");
	}

}


#endif //WIDGET_RUNTIME_SUPPORT
#endif //GADGET_UPDATE_SUP
