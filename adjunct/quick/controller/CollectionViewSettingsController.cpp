/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/controller/CollectionViewSettingsController.h"
#include "adjunct/quick/widgets/CollectionViewPane.h"
#include "adjunct/quick_toolkit/widgets/QuickOverlayDialog.h"
#include "adjunct/quick_toolkit/widgets/CalloutDialogPlacer.h"
#include "adjunct/quick_toolkit/widgets/QuickDropDown.h"
#include "adjunct/quick_toolkit/widgets/QuickButton.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "adjunct/desktop_util/prefs/PrefsCollectionDesktopUI.h"
#include "adjunct/quick/Application.h"

CollectionViewSettingsController::CollectionViewSettingsController(OpWidget* anchor_widget, CollectionViewPane* collection_view_pane)
	: Base(anchor_widget)
	, m_active_desktop_window(NULL)
	, m_collection_view_pane(collection_view_pane)
{
}

void CollectionViewSettingsController::InitL()
{
	QuickOverlayDialog* overlay_dlg = OP_NEW_L(QuickOverlayDialog, ());
	LEAVE_IF_ERROR(SetDialog("Collection View Settings Dialog", overlay_dlg));

	CalloutDialogPlacer* placer = OP_NEW_L(CalloutDialogPlacer, (*GetAnchorWidget()));
	LEAVE_IF_NULL(placer);

	overlay_dlg->SetDialogPlacer(placer);
	overlay_dlg->SetAnimationType(QuickOverlayDialog::DIALOG_ANIMATION_CALLOUT);

	GetAnchorWidget()->SetListener(this);

	QuickDropDown* dropdown = overlay_dlg->GetWidgetCollection()->Get<QuickDropDown>("sorting_dropdown");
	if (dropdown)
	{
		INT32 sort_type = g_pcui->GetIntegerPref(PrefsCollectionUI::CollectionSortType);
		dropdown->GetOpWidget()->SelectItem(sort_type, TRUE);
	}

	m_active_desktop_window = g_application->GetActiveDocumentDesktopWindow();
}

void CollectionViewSettingsController::OnUiClosing()
{
	GetAnchorWidget()->SetListener(NULL);
	Base::OnUiClosing();
}

void CollectionViewSettingsController::OnBeforePaint(OpWidget *widget)
{
	if (!m_active_desktop_window)
		return;

	/**
	 * Cancel the dialog if anchor widget is not in visible area.
	 * This is required if scroll action on a page (which hosts anchor
	 * widget) moves anchor widget out of visble-screen area.
	*/
	OpRect rect;
	m_active_desktop_window->GetBounds(rect);

	if (!rect.Intersecting(widget->GetRect(TRUE)))
		CancelDialog();
}

BOOL CollectionViewSettingsController::CanHandleAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_SWITCH_TO_THUMBNAIL_VIEW:
		case OpInputAction::ACTION_SWITCH_TO_LIST_VIEW:
		case OpInputAction::ACTION_SORT_COLLECTION_VIEW:
			 return TRUE;

		default:
			 return Base::CanHandleAction(action);
	}
}

BOOL CollectionViewSettingsController::DisablesAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_SORT_COLLECTION_VIEW:
			 return !m_collection_view_pane->IsSortOptionEnabled();

		default:
			 return Base::DisablesAction(action);
	}
}

BOOL CollectionViewSettingsController::SelectsAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_SWITCH_TO_LIST_VIEW:
			 return g_pcui->GetIntegerPref(PrefsCollectionUI::CollectionStyle) == 0;

		case OpInputAction::ACTION_SWITCH_TO_THUMBNAIL_VIEW:
			return g_pcui->GetIntegerPref(PrefsCollectionUI::CollectionStyle);

		default:
			 return Base::SelectsAction(action);
	}
}

OP_STATUS CollectionViewSettingsController::HandleAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_SWITCH_TO_THUMBNAIL_VIEW:
		case OpInputAction::ACTION_SWITCH_TO_LIST_VIEW:
			 m_collection_view_pane->SwitchViews(action->GetAction() == OpInputAction::ACTION_SWITCH_TO_THUMBNAIL_VIEW);
			 return OpStatus::OK;

		case OpInputAction::ACTION_SORT_COLLECTION_VIEW:
			 m_collection_view_pane->SortItems(static_cast<CollectionModelIterator::SortType>(action->GetActionData()));
			 return OpStatus::OK;

		default:
			 return Base::HandleAction(action);
	}
}
