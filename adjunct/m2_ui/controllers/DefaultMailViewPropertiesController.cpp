/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Alexander Remen (alexr@opera.com)
 *
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/desktop_util/prefs/PrefsCollectionM2.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2_ui/controllers/DefaultMailViewPropertiesController.h"
#include "adjunct/m2_ui/windows/MailDesktopWindow.h"
#include "adjunct/quick_toolkit/widgets/CalloutDialogPlacer.h"
#include "adjunct/quick_toolkit/widgets/QuickOverlayDialog.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/widgets/WidgetContainer.h"

#include "adjunct/m2/src/engine/engine.h"

DefaultMailViewPropertiesController::DefaultMailViewPropertiesController(OpWidget* anchor_widget, MailDesktopWindow* mail_window)
: PopupDialogContext(anchor_widget)
, m_mail_window(mail_window)
, m_overlay_dialog(NULL)
{
}

void DefaultMailViewPropertiesController::InitL()
{
	m_overlay_dialog = OP_NEW_L(QuickOverlayDialog, ());
	LEAVE_IF_ERROR(SetDialog("Mail Default View Properties Popup", m_overlay_dialog));

	m_overlay_dialog->SetAnimationType(QuickOverlayDialog::DIALOG_ANIMATION_CALLOUT);
	
	CalloutDialogPlacer* placer = OP_NEW_L(CalloutDialogPlacer, (*GetAnchorWidget()));
	m_overlay_dialog->SetDialogPlacer(placer);
	m_overlay_dialog->SetBoundingWidget(*m_mail_window->GetWidgetContainer()->GetRoot());
}

BOOL DefaultMailViewPropertiesController::CanHandleAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_MARK_AS_READ_AUTOMATICALLY:
		case OpInputAction::ACTION_SHOW_LIST_VIEW:
		case OpInputAction::ACTION_SHOW_SPLIT_VIEW:
		case OpInputAction::ACTION_SHOW_QUICK_REPLY:
		case OpInputAction::ACTION_SET_MAIL_DISPLAY_TYPE:
		case OpInputAction::ACTION_SET_MAIL_VIEW_TYPE:
		case OpInputAction::ACTION_GROUP_MAILS:
		case OpInputAction::ACTION_SORT_BY_COLUMN:
		case OpInputAction::ACTION_SORT_DIRECTION:
		case OpInputAction::ACTION_ENABLE_MSR:
		case OpInputAction::ACTION_EXPAND_MESSAGE:
		{
			return TRUE;
		}
	}
	return CloseDialogContext::CanHandleAction(action);
}

BOOL DefaultMailViewPropertiesController::DisablesAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_MARK_AS_READ_AUTOMATICALLY:
			return g_pcm2->GetIntegerPref(PrefsCollectionM2::MarkAsReadAutomatically) == action->GetActionData();
		case OpInputAction::ACTION_GROUP_MAILS:
			return g_pcm2->GetIntegerPref(PrefsCollectionM2::MailGroupingMethod) == action->GetActionData();
		case OpInputAction::ACTION_SORT_BY_COLUMN:
			return g_pcm2->GetIntegerPref(PrefsCollectionM2::DefaultMailSorting) == action->GetActionData();
	}
	return CloseDialogContext::DisablesAction(action);
}

BOOL DefaultMailViewPropertiesController::SelectsAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_SHOW_LIST_VIEW:
			return g_pcm2->GetIntegerPref(PrefsCollectionM2::MailViewMode) == MailLayout::LIST_ONLY;
		case OpInputAction::ACTION_SHOW_SPLIT_VIEW:
			return g_pcm2->GetIntegerPref(PrefsCollectionM2::MailViewMode) == action->GetActionData();
		case OpInputAction::ACTION_SHOW_QUICK_REPLY:
			return g_pcm2->GetIntegerPref(PrefsCollectionM2::ShowQuickReply) == 1;
		case OpInputAction::ACTION_SET_MAIL_DISPLAY_TYPE:
			return g_pcm2->GetIntegerPref(PrefsCollectionM2::MailBodyMode) == 0;
		case OpInputAction::ACTION_SET_MAIL_VIEW_TYPE:
			return g_pcm2->GetIntegerPref(PrefsCollectionM2::DefaultMailFlatThreadedView) == 1;
		case OpInputAction::ACTION_SORT_DIRECTION:
			return g_pcm2->GetIntegerPref(PrefsCollectionM2::DefaultMailSortingAscending) == 1;
		case OpInputAction::ACTION_ENABLE_MSR:
			return g_pcm2->GetIntegerPref(PrefsCollectionM2::FitToWidth) == 1;
		case OpInputAction::ACTION_EXPAND_MESSAGE:
		{
			PrefsCollectionM2::integerpref pref;
			if (g_pcm2->GetIntegerPref(PrefsCollectionM2::MailViewMode) == MailLayout::LIST_ON_LEFT_MESSAGE_ON_RIGHT && !m_mail_window->IsMessageOnlyView()) 
				pref = PrefsCollectionM2::MessageWidthListOnRight;
			else
				pref = PrefsCollectionM2::MessageWidthListOnTop;
			return g_pcm2->GetIntegerPref(pref) == 0;
		}
	
	}
	return DisablesAction(action);
}

OP_STATUS DefaultMailViewPropertiesController::HandleAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_MARK_AS_READ_AUTOMATICALLY:
		{
			if (g_pcm2->GetIntegerPref(PrefsCollectionM2::MarkAsReadAutomatically) != action->GetActionData())
			{
				RETURN_IF_LEAVE(g_pcm2->WriteIntegerL(PrefsCollectionM2::MarkAsReadAutomatically, action->GetActionData()));
				return OpStatus::OK;
			}
		}
		case OpInputAction::ACTION_SHOW_QUICK_REPLY:
		{
			RETURN_IF_LEAVE(g_pcm2->WriteIntegerL(PrefsCollectionM2::ShowQuickReply, !g_pcm2->GetIntegerPref(PrefsCollectionM2::ShowQuickReply)));
			return OpStatus::OK;
		}
		case OpInputAction::ACTION_SET_MAIL_DISPLAY_TYPE:
		{
			DialogPlacerUpdater update_placer(this);
			RETURN_IF_LEAVE(g_pcm2->WriteIntegerL(PrefsCollectionM2::MailBodyMode, !g_pcm2->GetIntegerPref(PrefsCollectionM2::MailBodyMode)));
			m_mail_window->ShowSelectedMail(FALSE, TRUE);
			return OpStatus::OK;
		}
		case OpInputAction::ACTION_SET_MAIL_VIEW_TYPE:
		{
			RETURN_IF_LEAVE(g_pcm2->WriteIntegerL(PrefsCollectionM2::DefaultMailFlatThreadedView, !g_pcm2->GetIntegerPref(PrefsCollectionM2::DefaultMailFlatThreadedView)));
			return g_m2_engine->ReinitAllIndexModels();
		}
		case OpInputAction::ACTION_GROUP_MAILS:
		{
			RETURN_IF_LEAVE(g_pcm2->WriteIntegerL(PrefsCollectionM2::MailGroupingMethod, action->GetActionData()));
			RETURN_IF_ERROR(g_m2_engine->ReinitAllIndexModels());
			return OpStatus::OK;
		}
		case OpInputAction::ACTION_SORT_BY_COLUMN:
		{
			int column = action->GetActionData();
			RETURN_IF_LEAVE(g_pcm2->WriteIntegerL(PrefsCollectionM2::DefaultMailSorting, column));
			RETURN_IF_ERROR(g_m2_engine->ReinitAllIndexModels());
			return OpStatus::OK;
		}
		case OpInputAction::ACTION_SORT_DIRECTION:
		{
			RETURN_IF_LEAVE(g_pcm2->WriteIntegerL(PrefsCollectionM2::DefaultMailSortingAscending, !g_pcm2->GetIntegerPref(PrefsCollectionM2::DefaultMailSortingAscending)));
			RETURN_IF_ERROR(g_m2_engine->ReinitAllIndexModels());
			return OpStatus::OK;
		}
		case OpInputAction::ACTION_ENABLE_MSR:
		{
			DialogPlacerUpdater update_placer(this);
			RETURN_IF_LEAVE(g_pcm2->WriteIntegerL(PrefsCollectionM2::FitToWidth, !g_pcm2->GetIntegerPref(PrefsCollectionM2::FitToWidth)));
			if (m_mail_window->GetWindowCommander())
			{
				m_mail_window->GetWindowCommander()->SetLayoutMode(g_pcm2->GetIntegerPref(PrefsCollectionM2::FitToWidth) ? OpWindowCommander::AMSR : OpWindowCommander::NORMAL);
			}
			return OpStatus::OK;
		}
		case OpInputAction::ACTION_SHOW_LIST_VIEW:
		{
			DialogPlacerUpdater update_placer(this);
			m_mail_window->OnInputAction(action);
			return OpStatus::OK;
		}
		case OpInputAction::ACTION_SHOW_SPLIT_VIEW:
		{
			DialogPlacerUpdater update_placer(this);
			m_mail_window->OnInputAction(action);
			return OpStatus::OK;
		}
		case OpInputAction::ACTION_EXPAND_MESSAGE:
		{
			PrefsCollectionM2::integerpref pref;
			if (g_pcm2->GetIntegerPref(PrefsCollectionM2::MailViewMode) == MailLayout::LIST_ON_LEFT_MESSAGE_ON_RIGHT && !m_mail_window->IsMessageOnlyView()) 
				pref = PrefsCollectionM2::MessageWidthListOnRight;
			else
				pref = PrefsCollectionM2::MessageWidthListOnTop;
	
			if (g_pcm2->GetIntegerPref(pref) != g_pcm2->GetDefaultIntegerPref(pref))
			{
				TRAPD(err, g_pcm2->ResetIntegerL(pref));
			}
			else
			{
				TRAPD(err, g_pcm2->WriteIntegerL(pref, g_pcm2->GetDefaultIntegerPref(pref) ? 0 : 105));
			}
			return TRUE;
		}
	}

	return CloseDialogContext::HandleAction(action);
}

DefaultMailViewPropertiesController::DialogPlacerUpdater::DialogPlacerUpdater(DefaultMailViewPropertiesController* controller) 
: m_controller(controller)
{
	m_controller->SetAnchorWidget(NULL);
	m_controller->m_overlay_dialog->SetDialogPlacer(NULL);
}

DefaultMailViewPropertiesController::DialogPlacerUpdater::~DialogPlacerUpdater()
{
	OpWidget* anchor = m_controller->m_mail_window->GetGlobalViewButton();
	if (anchor)
	{
		m_controller->SetAnchorWidget(anchor);
		CalloutDialogPlacer* placer = OP_NEW(CalloutDialogPlacer, (*anchor));
		if (!placer)
			return;
		m_controller->m_overlay_dialog->SetDialogPlacer(placer);
		m_controller->m_overlay_dialog->SetBoundingWidget(*m_controller->m_mail_window->GetWidgetContainer()->GetRoot());
	}
}

#endif // M2_SUPPORT
