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
#include "adjunct/m2/src/engine/index.h"
#include "adjunct/m2/src/engine/indexer.h"
#include "adjunct/m2_ui/controllers/IndexViewPropertiesController.h"
#include "adjunct/m2_ui/models/indexmodel.h"
#include "adjunct/m2_ui/windows/MailDesktopWindow.h"
#include "adjunct/quick_toolkit/bindings/QuickBinder.h"
#include "adjunct/quick_toolkit/widgets/CalloutDialogPlacer.h"
#include "adjunct/quick_toolkit/widgets/QuickLabel.h"
#include "adjunct/quick_toolkit/widgets/QuickOverlayDialog.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"

#include "modules/widgets/WidgetContainer.h"

IndexViewPropertiesController::IndexViewPropertiesController(OpWidget* anchor_widget, index_gid_t index_id, MailDesktopWindow* mail_window)
	: PopupDialogContext(anchor_widget)
	, m_index_id(index_id)
	, m_mail_window(mail_window)
	, m_index(NULL)
{
}

IndexViewPropertiesController::~IndexViewPropertiesController() 
{
	m_sort_ascending.Unsubscribe(m_index);
	m_custom_sorting.Unsubscribe(m_index);
	m_threaded.Unsubscribe(m_index);
	m_sorting.Unsubscribe(m_index);
	m_grouping.Unsubscribe(m_index);
}

void IndexViewPropertiesController::InitL()
{
	m_index = g_m2_engine->GetIndexById(m_index_id == IndexTypes::UNREAD_UI ? static_cast<index_gid_t>(IndexTypes::UNREAD) : m_index_id);
	
	if (!m_index)
		LEAVE(OpStatus::ERR_NULL_POINTER);

	QuickOverlayDialog* overlay_dialog = OP_NEW_L(QuickOverlayDialog, ());
	LEAVE_IF_ERROR(SetDialog("Mail Index View Properties Popup", overlay_dialog));

	CalloutDialogPlacer* placer = OP_NEW_L(CalloutDialogPlacer, (*GetAnchorWidget()));
	overlay_dialog->SetDialogPlacer(placer);
	overlay_dialog->SetBoundingWidget(*GetAnchorWidget()->GetParentDesktopWindow()->GetWidgetContainer()->GetRoot());
	overlay_dialog->SetAnimationType(QuickOverlayDialog::DIALOG_ANIMATION_CALLOUT);

	ANCHORD(OpString, string);
	ANCHORD(OpString, formatted_string);
	g_languageManager->GetStringL(Str::S_MAIL_SORTING_FOR_VIEW, string);
	LEAVE_IF_ERROR(formatted_string.AppendFormat(string.CStr(), m_index->GetName().CStr()));
	LEAVE_IF_ERROR(SetWidgetText<QuickLabel>("Sorting_for_view_label", formatted_string));

	if (!m_index->GetOverrideDefaultSorting())
	{
		m_index->SetModelType(static_cast<IndexTypes::ModelType>(g_pcm2->GetIntegerPref(PrefsCollectionM2::DefaultMailFlatThreadedView)), FALSE);
		m_index->SetModelGroup(g_pcm2->GetIntegerPref(PrefsCollectionM2::MailGroupingMethod));
		m_index->SetModelSort(g_pcm2->GetIntegerPref(PrefsCollectionM2::DefaultMailSorting));
		m_index->SetModelSortAscending(g_pcm2->GetIntegerPref(PrefsCollectionM2::DefaultMailSortingAscending) != 0);
	}

	LEAVE_IF_ERROR(InitOptions());
}

OP_STATUS IndexViewPropertiesController::InitOptions()
{
	bool is_overrided = m_index->GetOverrideDefaultSorting();
	RETURN_IF_ERROR(GetBinder()->Connect("Custom_sorting_checkbox", m_custom_sorting));
	m_custom_sorting.Set(is_overrided);
	RETURN_IF_ERROR(m_custom_sorting.Subscribe(MAKE_DELEGATE(*m_index, &Index::SetOverrideDefaultSorting)));
	RETURN_IF_ERROR(m_custom_sorting.Subscribe(MAKE_DELEGATE(*this, &IndexViewPropertiesController::ReinitTreeModel)));

	RETURN_IF_ERROR(GetBinder()->Connect("Threading_checkbox", m_threaded));
	m_threaded.Set((is_overrided ? m_index->GetModelType() : g_pcm2->GetIntegerPref(PrefsCollectionM2::DefaultMailFlatThreadedView)) == IndexTypes::MODEL_TYPE_THREADED);
	RETURN_IF_ERROR(m_threaded.Subscribe(MAKE_DELEGATE(*m_index, &Index::SetThreaded)));
	RETURN_IF_ERROR(m_threaded.Subscribe(MAKE_DELEGATE(*this, &IndexViewPropertiesController::ReinitTreeModel)));

	RETURN_IF_ERROR(GetBinder()->Connect("Grouping_dropdown", m_grouping));
	m_grouping.Set(is_overrided ? m_index->GetModelGrouping() :  g_pcm2->GetIntegerPref(PrefsCollectionM2::MailGroupingMethod));
	RETURN_IF_ERROR(m_grouping.Subscribe(MAKE_DELEGATE(*m_index, &Index::SetModelGroup)));
	RETURN_IF_ERROR(m_grouping.Subscribe(MAKE_DELEGATE(*this, &IndexViewPropertiesController::ReinitTreeModelInt)));

	RETURN_IF_ERROR(GetBinder()->Connect("Sorting_dropdown", m_sorting));
	m_sorting.Set(is_overrided ? m_index->GetModelSort() :  g_pcm2->GetIntegerPref(PrefsCollectionM2::DefaultMailSorting));
	RETURN_IF_ERROR(m_sorting.Subscribe(MAKE_DELEGATE(*m_index, &Index::SetModelSort)));
	RETURN_IF_ERROR(m_sorting.Subscribe(MAKE_DELEGATE(*this, &IndexViewPropertiesController::ReinitTreeModelInt)));

	RETURN_IF_ERROR(GetBinder()->Connect("SortDirection", m_sort_ascending));
	m_sort_ascending.Set(is_overrided ? m_index->GetModelSortAscending() :  g_pcm2->GetIntegerPref(PrefsCollectionM2::DefaultMailSortingAscending) != 0);
	RETURN_IF_ERROR(m_sort_ascending.Subscribe(MAKE_DELEGATE(*m_index, &Index::SetModelSortAscending)));
	RETURN_IF_ERROR(m_sort_ascending.Subscribe(MAKE_DELEGATE(*this, &IndexViewPropertiesController::ReinitTreeModel)));

	return InitShowFlags();
}

OP_STATUS IndexViewPropertiesController::InitShowFlags()
{
	for (int i = 0; i < IndexTypes::MODEL_FLAG_LAST; i++)
	{
		OpString8 checkbox_name;
		RETURN_IF_ERROR(checkbox_name.AppendFormat("cb_show_%d", i));
		RETURN_IF_ERROR(GetBinder()->Connect(checkbox_name.CStr(), m_show_flags[i]));
		m_show_flags[i].Set((m_index->GetModelFlags() & (1<<i)) != FALSE);
		RETURN_IF_ERROR(m_show_flags[i].Subscribe(MAKE_DELEGATE(*this, &IndexViewPropertiesController::UpdateShowFlags)));
	}
	return OpStatus::OK;
}

void IndexViewPropertiesController::ReinitTreeModel(bool)
{
	OpTreeModel* index_model;
	INT32 start_pos;

	if (OpStatus::IsSuccess(g_m2_engine->GetIndexModel(&index_model, m_index_id, start_pos)))
		static_cast<IndexModel*>(index_model)->ReInit();
	m_mail_window->UpdateSorting();
	OpStatus::Ignore(g_m2_engine->GetIndexer()->SaveRequest());
}

void IndexViewPropertiesController::UpdateShowFlags(bool)
{
	int flags = m_index->GetModelFlags();

	for (int i = 0; i < IndexTypes::MODEL_FLAG_LAST; i++)
	{
		flags = m_show_flags[i].Get() ? flags | (1<<i) : flags & ~(1 << i);
	}
	m_index->SetModelFlags(flags);
	
	if (m_index_id == IndexTypes::UNREAD_UI)
		g_m2_engine->GetIndexById(m_index_id)->SetModelFlags(flags);

	ReinitTreeModel(true);
}

#endif // M2_SUPPORT
