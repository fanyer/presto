/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/m2_ui/controllers/LabelPropertiesController.h"

#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/indexer.h"
#include "adjunct/m2/src/util/misc.h"
#include "adjunct/m2_ui/models/accessmodel.h"
#include "adjunct/m2_ui/models/accesstreeviewlistener.h"
#include "adjunct/quick_toolkit/bindings/QuickBinder.h"
#include "adjunct/quick_toolkit/widgets/QuickButton.h"
#include "adjunct/quick_toolkit/widgets/QuickDialog.h"
#include "adjunct/quick_toolkit/widgets/QuickEdit.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/QuickDynamicGrid.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/QuickStackLayout.h"
#include "adjunct/quick_toolkit/widgets/QuickTreeView.h"
#include "adjunct/quick_toolkit/widgets/QuickDropDown.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/desktop_util/file_chooser/file_chooser_fun.h"
#include "modules/locale/oplanguagemanager.h"


LabelPropertiesController::~LabelPropertiesController()
{
	Index* index = g_m2_engine->GetIndexById(m_current_id);
	if (index)
		DisconnectIndex(index);
	OP_DELETE(m_label_model);
	OP_DELETE(m_file_chooser);
	OP_DELETE(m_treeview_listener);
}

void LabelPropertiesController::InitL()
{
	LEAVE_IF_ERROR(SetDialog("Label Properties Dialog"));

	m_widgets = m_dialog->GetWidgetCollection();

	LEAVE_IF_ERROR(InitOptions());

	if (m_current_id == IndexTypes::SPAM)
	{
		QuickWidget* selector = m_widgets->GetL<QuickWidget>("LabelSelector");
		selector->Hide();

		LEAVE_IF_ERROR(GetBinder()->Connect("SpamInternalFilterLevel", m_spam_level));
		m_spam_level.Set(g_m2_engine->GetSpamLevel());
		LEAVE_IF_ERROR(m_spam_level.Subscribe(MAKE_DELEGATE(*g_m2_engine, &MessageEngine::SetSpamLevel)));

		LEAVE_IF_ERROR(RefreshLabelProperties());
	}
	else
	{
		QuickStackLayout* spam_section = m_widgets->GetL<QuickStackLayout>("SpamInternalFilter");
		spam_section->Hide();
		LEAVE_IF_ERROR(InitTreeView());
	}
}

OP_STATUS LabelPropertiesController::InitOptions()
{
	RETURN_IF_ERROR(GetBinder()->Connect("HideFromOtherViews", m_hide_from_other));
	RETURN_IF_ERROR(GetBinder()->Connect("MarkAsRead", m_mark_as_read));
	RETURN_IF_ERROR(GetBinder()->Connect("LearnFromMessages", m_learn_from_messages));
	RETURN_IF_ERROR(GetBinder()->Connect("NewMessagesOnly", m_new_messages_only));
	RETURN_IF_ERROR(GetBinder()->Connect("IMAPKeyword", m_imap_keyword));

	return OpStatus::OK;
}

OP_STATUS LabelPropertiesController::InitTreeView()
{
	QuickTreeView* treeview = m_widgets->Get<QuickTreeView>("LabelTreeView");
	m_label_model = OP_NEW(AccessModel, (IndexTypes::CATEGORY_LABELS, g_m2_engine->GetIndexer()));

	if (!treeview || !m_label_model)
		return OpStatus::ERR;

	RETURN_IF_ERROR(m_label_model->Init());

	OpTreeView* optreeview = treeview->GetOpWidget();
	optreeview->SetTreeModel(m_label_model, 0);
	optreeview->SetShowColumnHeaders(FALSE);
	optreeview->SetColumnVisibility(1, FALSE);
	optreeview->SetColumnVisibility(2, FALSE);
	optreeview->SetColumnVisibility(3, FALSE);
	optreeview->SetColumnVisibility(4, FALSE);
	optreeview->SetColumnVisibility(5, FALSE);
	optreeview->SetShowThreadImage(TRUE);
	optreeview->SetDragEnabled(TRUE);

	m_treeview_listener = OP_NEW(AccessTreeViewListener, (*optreeview));
	if (!m_treeview_listener)
		return OpStatus::ERR_NO_MEMORY;
	RETURN_IF_ERROR(treeview->AddOpWidgetListener(*m_treeview_listener));

	RETURN_IF_ERROR(RefreshLabelProperties());
	m_selected_label.Set(optreeview->GetModelIndexByIndex(optreeview->GetItemByID(m_current_id)));
	RETURN_IF_ERROR(m_selected_label.Subscribe(MAKE_DELEGATE(*this, &LabelPropertiesController::OnChangeLabel)));
	return GetBinder()->Connect(*treeview, m_selected_label);
}

void LabelPropertiesController::OnChangeLabel(INT32 new_label)
{
	Index* old_index = g_m2_engine->GetIndexById(m_current_id);
	if (old_index)
		DisconnectIndex(old_index);

	AccessModelItem* selected_item = m_label_model->GetItemByIndex(new_label);
	m_current_id = selected_item ? selected_item->GetID() : 0;
	OpStatus::Ignore(RefreshLabelProperties());
}

OP_STATUS LabelPropertiesController::RefreshLabelProperties()
{
	Index* index = g_m2_engine->GetIndexById(m_current_id);
	Index* search_index = GetSearchIndex(index);

	QuickStackLayout* mainview = m_widgets->Get<QuickStackLayout>("MainView");
	if (mainview)
		mainview->SetEnabled(index && search_index);

	if (!search_index || !index)
		return OpStatus::OK;

	RETURN_IF_ERROR(m_dialog->SetTitle(index->GetName().CStr()));
	RETURN_IF_ERROR(GetBinder()->Connect("LabelName", index->m_name));

	QuickDynamicGrid* rulegrid = m_widgets->Get<QuickDynamicGrid>("RulesGrid");
	if (!rulegrid)
		return OpStatus::ERR;

	for (unsigned i = 0; i < search_index->GetSearchCount(); i++)
	{
		IndexSearch* rule = search_index->GetM2Search(i);
		RETURN_IF_ERROR(ConnectRule(rulegrid, rule));
	}

	m_hide_from_other.Set(index->GetHideFromOther());
	m_hide_from_other.Subscribe(MAKE_DELEGATE(*index, &Index::SetHideFromOther));
	m_mark_as_read.Set(index->GetMarkMatchAsRead());
	m_mark_as_read.Subscribe(MAKE_DELEGATE(*index, &Index::SetMarkMatchAsRead));
	m_learn_from_messages.Set(index->GetHasAutoFilter());
	m_learn_from_messages.Subscribe(MAKE_DELEGATE(*index, &Index::SetHasAutoFilter));
	m_new_messages_only.Set(index->ShouldSearchNewMessagesOnly());
	m_new_messages_only.Subscribe(MAKE_DELEGATE(*index, &Index::SetSearchNewMessagesOnly));

	OpString keyword;
	OpStatus::Ignore(keyword.Set(index->GetKeyword()));
	m_imap_keyword.Set(keyword);

	Index* search_in_index;
	if (search_index != index)
	{
		search_in_index = g_m2_engine->GetIndexById(g_m2_engine->GetIndexer()->GetSearchInIndex(index->GetId()));
	}
	else
	{
		search_in_index = g_m2_engine->GetIndexById(IndexTypes::CATEGORY_MY_MAIL);

		if (!search_in_index)
		{
			search_in_index = g_m2_engine->GetIndexById(g_m2_engine->GetIndexer()->GetRSSAccountIndexId());
		}
	}
	RETURN_IF_ERROR(SetSeachInIndexTitle(search_in_index));

	return UpdateIcon(index);
}

void LabelPropertiesController::DisconnectIndex(Index* index)
{
	if (GetBinder())
		GetBinder()->Disconnect("LabelName");
	m_rules.DeleteAll();
	QuickDynamicGrid* rulegrid = m_widgets->Get<QuickDynamicGrid>("RulesGrid");
	if (rulegrid)
		rulegrid->Clear();

	if (!index)
		return;

	m_hide_from_other.Unsubscribe(index);
	m_mark_as_read.Unsubscribe(index);
	m_learn_from_messages.Unsubscribe(index);
	m_new_messages_only.Unsubscribe(index);

	OpString8 keyword;
	RETURN_VOID_IF_ERROR(OpMisc::EncodeKeyword(m_imap_keyword.Get(), keyword));
	g_m2_engine->GetIndexer()->SetKeyword(index, keyword);

	// Save changes to indexer
	RETURN_VOID_IF_ERROR(g_m2_engine->GetIndexer()->UpdateIndex(index->GetId()));
	Index* search_index = GetSearchIndex(index);
	if (search_index)
	{
		if (search_index->GetSearch()) 
		{
			if (!m_new_messages_only.Get())
			{
				search_index->GetSearch()->SetStartDate(0);
				search_index->GetSearch()->SetCurrentId((unsigned)-1); // updated again in UpdateIndex
			}
			search_index->GetSearch()->SetEndDate((unsigned)-1); // sensible end date for a filter search: never
		}
		RETURN_VOID_IF_ERROR(g_m2_engine->GetIndexer()->UpdateIndex(search_index->GetId()));
	}
}

OP_STATUS LabelPropertiesController::ConnectRule(QuickDynamicGrid* grid, IndexSearch* ruledata)
{
	OpAutoPtr<RuleUi> rule_ui (OP_NEW(RuleUi, ()));
	RETURN_OOM_IF_NULL(rule_ui.get());

	RETURN_IF_ERROR(grid->Instantiate(rule_ui->m_widgets));
	rule_ui->m_binder.SetWidgetCollection(&rule_ui->m_widgets);

	QuickButton* removebutton = rule_ui->m_widgets.Get<QuickButton>("RemoveRuleButton");
	if (removebutton && removebutton->GetAction())
		removebutton->GetAction()->SetActionData(reinterpret_cast<INTPTR>(rule_ui.get()));

	// The first rule must not have an operator
	if (m_rules.GetCount() == 0)
	{
		QuickDropDown* operator_dropdown = rule_ui->m_widgets.Get<QuickDropDown>("Operator");
		if (operator_dropdown)
			operator_dropdown->Hide();
	}

	RETURN_IF_ERROR(rule_ui->m_binder.Connect("Operator", ruledata->m_operator));
	RETURN_IF_ERROR(rule_ui->m_binder.Connect("Field", ruledata->m_search_body));
	RETURN_IF_ERROR(rule_ui->m_binder.Connect("MatchType", ruledata->m_option));
	RETURN_IF_ERROR(rule_ui->m_binder.Connect("Match", ruledata->m_search_text));

	RETURN_IF_ERROR(m_rules.Add(rule_ui.get()));
	rule_ui.release();

	return OpStatus::OK;
}

OP_STATUS LabelPropertiesController::UpdateIcon(Index* index)
{
	QuickButton* button = m_widgets->Get<QuickButton>("IconButton");
	if (!button)
		return OpStatus::ERR;

	const char* skinimage;
	Image customimage;
	RETURN_IF_ERROR(index->GetImages(skinimage, customimage));

	if (!customimage.IsEmpty())
		return button->SetImage(customimage);
	else
		return button->SetImage(skinimage);
}

BOOL LabelPropertiesController::CanHandleAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_NEW_FOLDER:
		case OpInputAction::ACTION_REMOVE_FILTER:
		case OpInputAction::ACTION_CHOOSE_LABEL_ICON:
		case OpInputAction::ACTION_CHOOSE_CUSTOM_ICON:
		case OpInputAction::ACTION_NEW_FILTER:
		case OpInputAction::ACTION_REMOVE_FOLDER:
		case OpInputAction::ACTION_SEARCH_IN_INDEX:
			return TRUE;
	}

	return CloseDialogContext::CanHandleAction(action);
}

BOOL LabelPropertiesController::DisablesAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_REMOVE_FOLDER:
			return m_current_id == 0;
	}

	return CloseDialogContext::DisablesAction(action);
}

OP_STATUS LabelPropertiesController::HandleAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_NEW_FOLDER:
			return AddNewLabel();
		case OpInputAction::ACTION_REMOVE_FOLDER:
			return RemoveLabel();
		case OpInputAction::ACTION_NEW_FILTER:
			return AddNewRule();
		case OpInputAction::ACTION_REMOVE_FILTER:
			return RemoveRule(reinterpret_cast<RuleUi*>(action->GetActionData()));
		case OpInputAction::ACTION_CHOOSE_LABEL_ICON:
			return SetIcon(action->GetActionImage());
		case OpInputAction::ACTION_CHOOSE_CUSTOM_ICON:
			return SelectCustomIcon();
		case OpInputAction::ACTION_SEARCH_IN_INDEX:
			return SelectSearchInIndex(action->GetActionData());
	}

	return CloseDialogContext::HandleAction(action);
}

OP_STATUS LabelPropertiesController::SelectSearchInIndex(index_gid_t index_id)
{
	Index *search_in_index = g_m2_engine->GetIndexById(index_id);
	if (!search_in_index)
		return OpStatus::OK;
	// searching in "all messages" is a special case, call with 0
	DisconnectIndex(g_m2_engine->GetIndexById(m_current_id));
	g_m2_engine->GetIndexer()->SetSearchInIndex(m_current_id, index_id == IndexTypes::CATEGORY_MY_MAIL ? 0 : index_id);
	return RefreshLabelProperties();
}

OP_STATUS LabelPropertiesController::SetSeachInIndexTitle(Index* index)
{
	QuickButton* button = m_widgets->Get<QuickButton>("SearchInMailboxButton");
	if (!button || !index)
		return OpStatus::ERR;

	const char* skinimage;
	Image customimage;
	RETURN_IF_ERROR(index->GetImages(skinimage, customimage));
	OpString name;
	if (index->GetId() >= IndexTypes::FIRST_CONTACT && index->GetId() < IndexTypes::LAST_CONTACT && index->GetSearch())
	{
		MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils()->GetContactName(index->GetSearch()->GetSearchText(), name);
		if (name.Compare(index->GetSearch()->GetSearchText()) == 0)
			name.Empty();
	}

	RETURN_IF_ERROR(button->SetText(name.HasContent() ? name : index->GetName()));

	if (!customimage.IsEmpty())
		return button->SetImage(customimage);
	else
		return button->SetImage(skinimage);
}

OP_STATUS LabelPropertiesController::AddNewLabel()
{
	index_gid_t parent = IndexTypes::CATEGORY_LABELS;
	if (m_current_id && m_current_id != IndexTypes::SPAM)
		parent = m_current_id;

	Index* index = g_m2_engine->CreateIndex(parent, TRUE);
	if (!index)
		return OpStatus::ERR_NO_MEMORY;

	AccessModelItem* item = m_label_model->GetItemByID(index->GetId());
	if (item)
		m_selected_label.Set(item->GetIndex());

	QuickEdit* labelname = m_widgets->Get<QuickEdit>("LabelName");
	if (labelname)
		labelname->GetOpWidget()->SetFocus(FOCUS_REASON_OTHER);

	return OpStatus::OK;
}

OP_STATUS LabelPropertiesController::RemoveLabel()
{
	index_gid_t remove = m_current_id;
	m_selected_label.Set(-1);
	return g_m2_engine->RemoveIndex(remove);
}

OP_STATUS LabelPropertiesController::AddNewRule()
{
	Index* index = GetSearchIndex(g_m2_engine->GetIndexById(m_current_id));
	if (!index)
		return OpStatus::ERR;

	IndexSearch* search = index->AddM2Search();
	QuickDynamicGrid* rulegrid = m_widgets->Get<QuickDynamicGrid>("RulesGrid");
	if (!search || !rulegrid)
		return OpStatus::ERR;

	return ConnectRule(rulegrid, search);
}

OP_STATUS LabelPropertiesController::RemoveRule(RuleUi* rule_ui)
{
	OP_ASSERT(rule_ui != NULL);

	const INT32 pos = m_rules.Find(rule_ui);
	OP_ASSERT(pos >= 0);

	m_rules.Delete(pos);

	Index* index = GetSearchIndex(g_m2_engine->GetIndexById(m_current_id));
	QuickDynamicGrid* rulegrid = m_widgets->Get<QuickDynamicGrid>("RulesGrid");
	if (!index || !rulegrid)
		return OpStatus::ERR;

	rulegrid->RemoveRow(pos);
	RETURN_IF_ERROR(index->RemoveSearch(pos));

	// The first rule must not have an operator
	if (pos == 0 && m_rules.GetCount() >= 1)
	{
		RuleUi* rule_ui = m_rules.Get(pos);
		QuickDropDown* operator_dropdown = rule_ui->m_widgets.Get<QuickDropDown>("Operator");
		if (operator_dropdown)
			operator_dropdown->Hide();
	}

	return OpStatus::OK;
}

Index* LabelPropertiesController::GetSearchIndex(Index* index)
{
	if (index && index->GetSearchIndexId() != 0)
		index = g_m2_engine->GetIndexById(index->GetSearchIndexId());

	return index;
}

OP_STATUS LabelPropertiesController::SetIcon(const char* icon)
{
	Index *index = g_m2_engine->GetIndexById(m_current_id);
	if (!index)
		return OpStatus::OK;

	index->SetSkinImage(icon);
	return UpdateIcon(index);
}

OP_STATUS LabelPropertiesController::SelectCustomIcon()
{
	if (!m_file_chooser)
		RETURN_IF_ERROR(DesktopFileChooser::Create(&m_file_chooser));

	m_request.action = DesktopFileChooserRequest::ACTION_FILE_OPEN;

	OpString filter;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::D_SELECT_CUSTOM_ICON, m_request.caption));
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_FILE_CHOOSER_IMAGE_FILTER, filter));

	if (filter.HasContent())
		RETURN_IF_ERROR(StringFilterToExtensionFilter(filter.CStr(), &m_request.extension_filters));

	return m_file_chooser->Execute(m_dialog->GetDesktopWindow()->GetOpWindow(), this, m_request);
}

void LabelPropertiesController::OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result)
{
	if (result.files.GetCount() == 0)
		return;

	Index* index = g_m2_engine->GetIndexById(m_current_id);
	if (!index)
		return;
	index->SetCustomImage(*result.files.Get(0));
	OpStatus::Ignore(UpdateIcon(index));
}
