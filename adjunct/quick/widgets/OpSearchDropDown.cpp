/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/quick/widgets/OpSearchDropDown.h"

#include "modules/doc/doc.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/widgets/OpEdit.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/dragdrop/dragdrop_manager.h"

#ifdef DESKTOP_UTIL_SEARCH_ENGINES
#include "adjunct/desktop_util/search/search_net.h"
#include "adjunct/desktop_util/search/searchenginemanager.h"
#endif // DESKTOP_UTIL_SEARCH_ENGINES
#include "adjunct/quick/managers/FavIconManager.h"
#include "adjunct/quick_toolkit/widgets/OpToolbar.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/quick/Application.h"
#ifdef ENABLE_USAGE_REPORT
#include "adjunct/quick/usagereport/UsageReport.h"
#endif
#include "adjunct/desktop_util/search/search_field_history.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/desktop_util/skin/SkinUtils.h"

#define OPSEARCHDROPDOWN_WIDTH 180
#define MANAGE_SEARCHES_INDEX 5000 ///< Special index for the manage searches item

#ifndef SEARCH_SUGGEST_DELAY
#define SEARCH_SUGGEST_DELAY	250		// wait 250ms after the last letter has been entered before doing a search suggestion call
#endif // SEARCH_SUGGEST_DELAY

#define SEARCH_COLUMN_INDENTATION		0
#define ICON_COLUMN_WIDTH 				34


/***********************************************************************************
 **
 **	OpSearchDropDown
 **
 ** Note: UserData in the menu item is set to the index of the search
 **
 **
 ***********************************************************************************/
DEFINE_CONSTRUCT(OpSearchDropDown)

OpSearchDropDown::OpSearchDropDown() :
	m_type(-1),
	m_search_changed(TRUE),
	m_search_dropdown_listener(0),
	m_search_suggest_in_progress(FALSE),
	m_search_suggest(NULL),
	m_is_inside_speeddial(FALSE),
	m_follow_default_search(TRUE),
	m_mh(NULL),
	m_query_error_counter(0),
	m_active_search_item(NULL),
	m_web_search_item(NULL)
{
	RETURN_VOID_IF_ERROR(init_status);

	GetBorderSkin()->SetImage("Dropdown Search Skin", "Dropdown Skin");
	SetSpyInputContext(this, FALSE);
	SetListener(this);

	if (GetTargetDocumentDesktopWindow())
	{
		OpBrowserView* browser_view = GetTargetDocumentDesktopWindow()->GetBrowserView();

		m_search_guid.Set(browser_view->GetSearchGUID());
	}

	ValidateSearchType();
	UpdateSearchField();

	if (GetTargetDocumentDesktopWindow())
	{
		UpdateFollowDefaultSearch();
	}

	SetExtraLineHeight(4);

	SetAllowWrappingOnScrolling(TRUE);

	SetDeselectOnScrollEnd(FALSE);

	if (!m_mh)
		m_mh = OP_NEW(MessageHandler, (NULL));
	if (!m_mh)
	{
		init_status = OpStatus::ERR_NO_MEMORY;
		return;
	}

	m_search_suggest = OP_NEW(SearchSuggest, (this));	

	m_mh->SetCallBack(this, MSG_QUICK_START_SEARCH_SUGGESTIONS, 0);
	m_mh->SetCallBack(this, MSG_QUICK_REMOVE_SEARCH_HISTORY_ITEM, 0);
}

OpSearchDropDown::~OpSearchDropDown()
{
	if (m_mh)
	{
		m_mh->UnsetCallBacks(this);
		OP_DELETE(m_mh);
	}
	OP_DELETE(m_search_suggest);
}

void OpSearchDropDown::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	GetInfo()->GetPreferedSize(this, OpTypedObject::WIDGET_TYPE_DROPDOWN, w, h, cols, rows);
	// this is the hardcoded width of the search drop down:
	if (OPSEARCHDROPDOWN_WIDTH > *w)
		*w = OPSEARCHDROPDOWN_WIDTH;
}

void OpSearchDropDown::OnDeleted()
{
	SetSpyInputContext(NULL, FALSE);

	OpTreeViewDropdown::OnDeleted();
}

void OpSearchDropDown::SetSearchInSpeedDial(BOOL is_inside_speeddial) 
{ 
	m_is_inside_speeddial = is_inside_speeddial; 

	if(m_is_inside_speeddial)
	{
		SearchTemplate *search = g_searchEngineManager->GetDefaultSpeedDialSearch();
		if(search)
		{
			search->GetUniqueGUID(m_search_guid);
		}
	}
	else
	{
		if (GetTargetDocumentDesktopWindow())
		{
			OpBrowserView* browser_view = GetTargetDocumentDesktopWindow()->GetBrowserView();

			m_search_guid.Set(browser_view->GetSearchGUID());
		}
		m_follow_default_search = TRUE; // UpdateFollowDefaultSearch will verify this
	}
	m_search_changed = TRUE;

	ValidateSearchType();
	UpdateSearchField();
	UpdateTargetDocumentDesktopWindow();	// See DSK-309272, needed because of timing issues
	UpdateFollowDefaultSearch();
}

/***********************************************************************************
**
**	Search
**
***********************************************************************************/

void OpSearchDropDown::Search(SearchEngineManager::SearchSetting settings, BOOL is_suggestion)
{
	if (GetTargetDocumentDesktopWindow())
		settings.m_target_window = GetTargetDocumentDesktopWindow();
	else if (g_application->GetActiveDocumentDesktopWindow())
	{
		// This happens when search dropdown is used in web-search dialog
		settings.m_target_window = g_application->GetActiveDocumentDesktopWindow();
	}

	if (settings.m_keyword.IsEmpty())
		GetText(settings.m_keyword);

	settings.m_keyword.Strip(); // No web search with empty strings

	if (settings.m_keyword.HasContent())
	{
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
		if (!settings.m_search_template)
			settings.m_search_template = g_searchEngineManager->GetByUniqueGUID(m_search_guid);
		if (settings.m_search_template)
		{
			settings.m_search_issuer = m_is_inside_speeddial ?
				SearchEngineManager::SEARCH_REQ_ISSUER_SPEEDDIAL : SearchEngineManager::SEARCH_REQ_ISSUER_OTHERS;
			if (g_pcdoc->GetIntegerPref(PrefsCollectionDoc::NewWindow) && GetParentDesktopWindowType() != WINDOW_TYPE_DOCUMENT)
				settings.m_target_window = NULL;

			g_searchEngineManager->DoSearch(settings);
		}

		BOOL is_privacy_mode = (GetTargetDocumentDesktopWindow() && GetTargetDocumentDesktopWindow()->PrivacyMode()) || 
			(g_application->GetActiveBrowserDesktopWindow() && g_application->GetActiveBrowserDesktopWindow()->PrivacyMode());

		if (!is_privacy_mode)
		{
#ifdef ENABLE_USAGE_REPORT
			if(settings.m_search_template && g_usage_report_manager && g_usage_report_manager->GetSearchReport())
			{
				SearchReport::SearchAccessPoint access_point;
				// deduce search type.
				if (m_is_inside_speeddial)
					access_point = SearchReport::SearchSpeedDial;
				else
					access_point = SearchReport::SearchFromSearchfield;

				// suggestions are offsetted from access point's value + SearchSuggestionOffset.
				// that is, for instance, SearchSuggestedFromSpeedDial has the value SearchSpeedDial + SearchSuggestionOffset.
				if (is_suggestion)
					access_point = (SearchReport::SearchAccessPoint)(SearchReport::SearchSuggestionOffset + access_point);

				g_usage_report_manager->GetSearchReport()->AddSearch(access_point, GetSearchGUID());
			}
#endif // ENABLE_USAGE_REPORT
			OpString guid;

			// add it to the list of the last searches
			if(settings.m_search_template && OpStatus::IsSuccess(settings.m_search_template->GetUniqueGUID(guid)))
			{
				if(OpStatus::IsSuccess(g_search_field_history->AddEntry(guid.CStr(), settings.m_keyword.CStr())))
				{
					OpStatus::Ignore(g_search_field_history->Write());
				}
			}
		}

#endif // DESKTOP_UTIL_SEARCH_ENGINES
		if (m_search_dropdown_listener)
			m_search_dropdown_listener->OnSearchDone();
	}
}

/***********************************************************************************
**
**	UpdateSearchField
**
***********************************************************************************/

void OpSearchDropDown::UpdateSearchField(SearchTemplate *search)
{
	if (!m_is_inside_speeddial && GetTargetDocumentDesktopWindow())
	{
		OpBrowserView* browser_view = GetTargetDocumentDesktopWindow()->GetBrowserView();
		browser_view->SetSearchGUID(m_search_guid);
	}

#ifdef DESKTOP_UTIL_SEARCH_ENGINES
	if(!search)
	{
		if(m_is_inside_speeddial)
		{
			search = g_searchEngineManager->GetDefaultSpeedDialSearch();
		}
		else
		{
			search = g_searchEngineManager->GetByUniqueGUID(m_search_guid);
		}
	}

	m_type = search ? search->GetSearchType() : -1;
	edit->autocomp.SetType(m_type == SEARCH_TYPE_GOOGLE ? AUTOCOMPLETION_GOOGLE : AUTOCOMPLETION_OFF);

	SetAction(OP_NEW(OpInputAction, ( OpInputAction::ACTION_GO )) );

#endif // DESKTOP_UTIL_SEARCH_ENGINES

	// Update ghost string
	OpString buf;
	TRAPD(err, buf.ReserveL(64));
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
	if (m_search_guid.HasContent())
		g_searchEngineManager->MakeSearchWithString(g_searchEngineManager->GetByUniqueGUID(m_search_guid), buf);
#endif // DESKTOP_UTIL_SEARCH_ENGINES

	OpString dummy;

	RETURN_VOID_IF_ERROR(CreateFormattingString(Str::S_SEARCH_FIELD_SEARCH_WITH, buf, dummy, m_ghost_string));

	SetGhostText(m_ghost_string.CStr());
	edit->SetShowGhostTextWhenFocused(TRUE);
	edit->SetGhostTextJusifyWhenFocused(JUSTIFY_LEFT);
	edit->SetToolTipText(m_ghost_string);
	
	UINT32 color;
	if (OpStatus::IsSuccess(g_skin_manager->GetTextColor("Ghost Text Foreground Skin", &color, 0)))
		edit->SetGhostTextFGColorWhenFocused(color);
	edit->SetHasCssBackground(TRUE);
	edit->GetBorderSkin()->SetImage("Search Dropdown Edit Skin","Edit Skin");

	UpdateSearchIcon();
}
/* static */
OP_STATUS OpSearchDropDown::CreateFormattingString(Str::LocaleString message_id, OpString& content_string, OpString& content_string_2, OpString& final_string)
{
	OpString text;
	int expected_len;

	g_languageManager->GetString(message_id, text);

	expected_len = text.Length() + 256;
	uni_char* mess_p = final_string.Reserve(expected_len);
	if (!mess_p)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	uni_snprintf_ss(mess_p, expected_len, text.CStr(), content_string.CStr(), content_string_2.CStr());

	return OpStatus::OK;
}

/***********************************************************************************
**
**	UpdateSearchIcon
**
***********************************************************************************/

void OpSearchDropDown::UpdateSearchIcon()
{
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
	if (m_search_changed)
	{
		if(GetParent() && GetParent()->GetType() == WIDGET_TYPE_SEARCH_SPECIAL_DROPDOWN)
		{
			((OpSearchDropDownSpecial *)GetParent())->UpdateSearchIcon();
		}
		m_search_changed = FALSE;
	}
#endif // DESKTOP_UTIL_SEARCH_ENGINES
}

/***********************************************************************************
 **
 **	SetSearchType
 **
 ** @param type - a real search type
 ** 
 ** Sets m_search_index to the index of the search with type type
 **
 ***********************************************************************************/

#ifdef DESKTOP_UTIL_SEARCH_ENGINES
void OpSearchDropDown::SetSearchFromType(SearchType type)
{
	int idx = g_searchEngineManager->SearchTypeToIndex(type);

	SearchTemplate* search = g_searchEngineManager->GetSearchEngine(idx);
	if (search && search->GetUniqueGUID().Compare(m_search_guid))
	{
		m_search_guid.Set(search->GetUniqueGUID());
		m_search_changed = TRUE;
	}

	ValidateSearchType();
	UpdateSearchField();
	UpdateFollowDefaultSearch();
}
#endif // DESKTOP_UTIL_SEARCH_ENGINES

/**********************************************************************************
 *
 * ValidateSearchType
 *
 *
 *
 ***********************************************************************************/
void OpSearchDropDown::ValidateSearchType()
{
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
	SearchTemplate* search = g_searchEngineManager->GetByUniqueGUID(m_search_guid);
	if (!search || search->GetKey().IsEmpty())
	{
		m_search_guid.Empty();
		m_search_changed = TRUE;
	}
#endif // DESKTOP_UTIL_SEARCH_ENGINES

}

void OpSearchDropDown::UpdateSelectedItem()
{
	OpString search_text;
	GetText(search_text);
	SearchDropDownModelItem *item = static_cast<SearchDropDownModelItem *>(GetSelectedItem());
	SearchTemplate *search_item = NULL;
	m_active_search_item = item;
	if(!item)
		return;

	// regular search engine
	if(item->GetSearchItem())
	{
		search_item = item->GetSearchItem();
		if (!m_search_text.HasContent()) {
			m_search_text.Set(search_text);
		}
		SetText(m_search_text.CStr());
	}
	// search suggestion item
	else
	{
		if (!m_search_text.HasContent())
			m_search_text.Set(search_text);
		switch (item->GetType())
		{
			case OpTypedObject::WIDGET_TYPE_SEARCH_SUGGESTION_ITEM:
			{
				SearchSuggestionModelItem *s_item = static_cast<SearchSuggestionModelItem *>(item);
				search_item = g_searchEngineManager->GetByUniqueGUID(s_item->GetSearchProvider());
				if(search_item)
				{
					search_text.Set(s_item->GetSearchSuggestion());
				}
				break;
			}
			case OpTypedObject::WIDGET_TYPE_SEARCH_HISTORY_ITEM:
			{
				SearchHistoryModelItem *s_item = static_cast<SearchHistoryModelItem *>(item);
				search_item = g_searchEngineManager->GetByUniqueGUID(s_item->GetSearchID());
				if(search_item)
				{
					search_text.Set(s_item->GetSearchTerm());
				}
				break;
			}
			case OpTypedObject::WIDGET_TYPE_SEARCH_ACTION_ITEM:
				search_item = g_searchEngineManager->GetByUniqueGUID(m_search_guid);
				m_active_search_item = NULL;
				search_text.Set(m_search_text);
				break;
			default:
				m_active_search_item = NULL;
				break;
		}
		SetText(search_text.CStr());
	}
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL OpSearchDropDown::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
			{
				OpInputAction* child_action = action->GetChildAction();

				switch (child_action->GetAction())
				{
					case OpInputAction::ACTION_STOP:
						child_action->SetEnabled(edit->IsFocused());
						return TRUE;

					case OpInputAction::ACTION_SHOW_POPUP_MENU:
						child_action->SetEnabled(edit->IsFocused());
						return TRUE;
				}
				return FALSE;
			}

		case OpInputAction::ACTION_DELETE_SELECTED_ITEM:
			{
				SearchDropDownModelItem *item = static_cast<SearchDropDownModelItem *>(GetSelectedItem());
				if(item && item->IsUserDeletable())
				{
					// we want to remove the history item
					m_mh->PostDelayedMessage(MSG_QUICK_REMOVE_SEARCH_HISTORY_ITEM, (MH_PARAM_1)item, 0, 0);
				}
			}
			break;

		case OpInputAction::ACTION_PREVIOUS_ITEM:
		case OpInputAction::ACTION_NEXT_ITEM:
			if (!DropdownIsOpen())
			{
				ShowMenu();
				return TRUE;
			}
			else
			{
				// Let the base do the action first to update the selected item
				OpTreeViewDropdown::OnInputAction(action);

				UpdateSelectedItem();

				return TRUE;
			}
			break;

		case OpInputAction::ACTION_GO:
			{
				SearchDropDownModelItem *selected_item = static_cast<SearchDropDownModelItem *>(GetSelectedItem());
				if(selected_item && 
					(selected_item->GetType() == OpTypedObject::WIDGET_TYPE_SEARCH_ACTION_ITEM 
					|| selected_item->GetType() == OpTypedObject::WIDGET_TYPE_SEARCH_SUGGESTION_ITEM
					|| selected_item->GetType() == OpTypedObject::WIDGET_TYPE_SEARCH_HISTORY_ITEM))
				{
					ExecuteSearchForItem(selected_item);
				}
				else
				{
					selected_item = GetActiveSearchEngineItem();
					if(selected_item)
					{
						ExecuteSearchForItem(selected_item);
					}
					else
					{
						SearchEngineManager::SearchSetting settings;
						g_application->AdjustForAction(settings, action, GetTargetDocumentDesktopWindow());

						m_search_changed = TRUE;
						UpdateSearchField();
						Search(settings, FALSE);
					}
				}
				return TRUE;
			}

		// Just so that we can Escape into the document
		case OpInputAction::ACTION_STOP:
		{
			if (action->IsKeyboardInvoked() && GetTargetDocumentDesktopWindow() && !GetTargetDocumentDesktopWindow()->GetWindowCommander()->IsLoading() && edit->IsFocused())
			{
				// If the dropdown is open - close it :
				if (DropdownIsOpen())
				{
					ShowDropdown(FALSE);
					return TRUE;
				}
				ReleaseFocus(FOCUS_REASON_OTHER);
				g_input_manager->InvokeAction(OpInputAction::ACTION_FOCUS_PAGE);
				return TRUE;
			}
			break;
		}
		case OpInputAction::ACTION_SHOW_POPUP_MENU:
		{
			OpRect rect;

			action->GetActionPosition(rect);

			OnContextMenu(this, 0, OpPoint(rect.x, rect.y), NULL, action->IsKeyboardInvoked());

			return TRUE;
		}
	}
	return OpTreeViewDropdown::OnInputAction(action);
}


/***********************************************************************************
**
**	OnTargetDocumentDesktopWindowChanged
**
***********************************************************************************/

void OpSearchDropDown::OnTargetDocumentDesktopWindowChanged(DocumentDesktopWindow* target_window)
{
	// ignore this if it's the speed dial search
	if (target_window && !m_is_inside_speeddial)
	{
		OpBrowserView* browser_view = target_window->GetBrowserView();
		if (!browser_view)
			return;
		m_search_guid.Set(browser_view->GetSearchGUID());
		m_search_changed = TRUE;
		ValidateSearchType();
	}
	UpdateSearchField();
	UpdateFollowDefaultSearch();
}

/***********************************************************************************
**
**	OnChange
**
***********************************************************************************/

void OpSearchDropDown::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (widget == edit)
	{
		OpString keyword;
		GetText(keyword);

		SetUserString(keyword.CStr());
		m_search_text.Set(keyword);

		keyword.Strip(); // No web search with empty strings

#ifdef DESKTOP_UTIL_SEARCH_ENGINES
		if (!DropdownIsOpen())
		{
			ShowMenu();
		}

		if (m_web_search_item)
		{
			m_web_search_item->SetActiveSearch(TRUE,m_search_text.CStr());
		}

		SearchDropDownModel* model = GetModel();
		const BOOL has_model = model != NULL;

		// TODO: Make some proper detection for the engines supported
		if (!keyword.HasContent())
		{
			if (has_model && model->GetItemCount() > 0)
			{
				for (SearchDropDownModelItem *item = model->GetItemByIndex(0);
					 NULL != item && item->IsSuggestion();)
				{
					item->Delete();
					item = model->GetItemByIndex(0);
				}
				ResizeDropdown();
			}
			m_query_error_counter = 0;
		}
		else if(m_search_suggest->HasSuggest(m_search_guid.CStr()))
		{
			StartSearchSuggest();
		}
		if(has_model)
		{
			AddSearchHistoryItems(keyword);
			EnsureSeparator();
			ResizeDropdown();
		}
#endif // DESKTOP_UTIL_SEARCH_ENGINES
		if (GetParent())
			GetParent()->OnContentSizeChanged();
	}
}

/***********************************************************************************
**
**	OnMouseEvent
**
***********************************************************************************/

void OpSearchDropDown::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if (widget == edit)
	{
		OpTreeViewDropdown::OnMouseEvent(widget, pos, x, y, button, down, nclicks);
		return;
	}

	if (!down) // we skip OnMouseDown event to avoid acting twice
	{
		SearchDropDownModelItem *item = static_cast<SearchDropDownModelItem *>(GetSelectedItem());

		if(nclicks == 1 && item && item->IsUserDeletable() &&
			widget->GetType() == WIDGET_TYPE_TREEVIEW &&
			static_cast<OpTreeView *>(widget)->IsHoveringAssociatedImageItem(pos))
		{
			// we clicked the close button on the history item, remove it
			m_mh->PostDelayedMessage(MSG_QUICK_REMOVE_SEARCH_HISTORY_ITEM, (MH_PARAM_1)item, 0, 0);
			return;
		}

		ExecuteSearchForItem(item);
	}

	if (widget == this)
		static_cast<OpWidgetListener *>(GetParent())->OnMouseEvent(this, pos, x, y, button, down, nclicks);
}

/***********************************************************************************
**
**	OnDropdownMenu
**
***********************************************************************************/

void OpSearchDropDown::OnDropdownMenu(OpWidget *widget, BOOL show)
{
	if (show)
	{
		//--------------------------------------------------
		// Initialize the dropdown :
		//--------------------------------------------------

		RETURN_VOID_IF_ERROR(InitTreeViewDropdown("Search Treeview Window Skin"));
		GetTreeView()->SetOnlyShowAssociatedItemOnHover(TRUE);

		//--------------------------------------------------
		// Make sure we have a model :
		//--------------------------------------------------

		// The dropdown is responsible for deleting the items
		BOOL items_deletable = TRUE;

		SearchDropDownModel* tree_model = OP_NEW(SearchDropDownModel, ());
		if (!tree_model)
			return;

		SetModel(tree_model, items_deletable);
		SetTreeViewName(WIDGET_NAME_MAINSEARCH_TREEVIEW);

		//--------------------------------------------------
		// Get the previous text in the search field, it's used by some of the code
		//--------------------------------------------------
		OpString search_text;

		GetText(search_text);

		//--------------------------------------------------
		// Start the suggest if we have a text
		//--------------------------------------------------
		// TODO: Make some proper detection for the engines supported
		if(search_text.HasContent() && m_search_suggest->HasSuggest(m_search_guid.CStr()))
		{
			StartSearchSuggest();
		}

		//--------------------------------------------------
		// Add search history entries - filtered on the search text
		//--------------------------------------------------
		// 
		AddSearchHistoryItems(search_text);

		//--------------------------------------------------
		// Add search engines if not inside Speed Dial
		//--------------------------------------------------
		AddSearchEngines(search_text);

		//--------------------------------------------------
		// Add a shortcut to the prefs for editing search engines
		//--------------------------------------------------
		AddManageSearchEngines();

		//--------------------------------------------------
		// Add a separator if needed
		//--------------------------------------------------
		EnsureSeparator();

		GetTreeView()->SetColumnFixedWidth(0, ICON_COLUMN_WIDTH);
		GetTreeView()->SetColumnWeight(1, 1);
		GetTreeView()->SetColumnFixedCharacterWidth(2, 5);

		SearchDropDownModel* model = GetModel();
		SetMaxLines(model->GetCount());
		ShowDropdown(model->GetCount() > 0);
	}
	else
	{
		Clear();

		if( m_search_dropdown_listener )
		{
			m_search_dropdown_listener->OnSearchChanged(m_search_guid);
		}
	}
}


/***********************************************************************************
**
**	OnDragStart
**
***********************************************************************************/

void OpSearchDropDown::OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y)
{
	if (!g_application->IsDragCustomizingAllowed())
		return;

	// If the OpSearchDropDown is a child of the OpResizeSearchDropDown pass on the OnDragStart
	// So the control can be dragged. This is needed because listeners only work for one level of children
	if (GetParent() && GetParent()->GetParent() && GetParent()->GetParent()->GetType() == WIDGET_TYPE_RESIZE_SEARCH_DROPDOWN)
	{
		OpPoint point(x, y);
		GetParent()->GetParent()->OnDragStart(point);
	}
	else
	{
		DesktopDragObject* drag_object = GetDragObject(OpTypedObject::DRAG_TYPE_SEARCH_DROPDOWN, x, y);

		if (drag_object)
		{
			drag_object->SetObject(this);
			g_drag_manager->StartDrag(drag_object, NULL, FALSE);
		}
	}
}

void OpSearchDropDown::OnClear()
{
	m_web_search_item = NULL;
	m_active_search_item = NULL;
	OpTreeViewDropdown::OnClear();
}

void OpSearchDropDown::OnInvokeAction(BOOL invoked_on_user_typed_string)
{
	OpTreeViewDropdown::OnInvokeAction(invoked_on_user_typed_string);
}

void OpSearchDropDown::ShowDropdown(BOOL show)
{
	m_active_search_item = NULL;
	OpTreeViewDropdown::ShowDropdown(show);
}

void OpSearchDropDown::StartSearchSuggest()
{
	m_mh->RemoveDelayedMessage(MSG_QUICK_START_SEARCH_SUGGESTIONS, 0, 0);
	int n = 2 << m_query_error_counter;
	if (n > 25)
		n = 25;
	m_mh->PostDelayedMessage(MSG_QUICK_START_SEARCH_SUGGESTIONS, 0, 0, 100 * n);
}


void OpSearchDropDown::EnsureSeparator()
{
	SearchDropDownModel* model = GetModel();
	if (!model)
		return;

	SearchDropDownModelItem *separator_item = NULL;

	for(INT32 index = 0; index < model->GetCount(); index++)
	{
		SearchDropDownModelItem *item = model->GetItemByIndex(index);
		if (item->IsSeparator())
		{
			separator_item = item;
			break;
		}
	}

	if (separator_item != NULL)
	{
		 if (model->GetItemCount() > 2)
		 {
			 return;
		 }
		 else
			separator_item->Delete();
	}

	if (model->GetItemCount() < 2)
		return;

	SearchDropDownModelItem* item = OP_NEW(SearchDropDownModelItem, ());
	if (!item)
	{
		return;
	}
	item->SetSeparator(TRUE);

	if (model->InsertBefore(item, GetModel()->GetItemCount()-1) == -1)
	{
		OP_DELETE(item);
	}

	return;
}


void OpSearchDropDown::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if(msg == MSG_QUICK_START_SEARCH_SUGGESTIONS)
	{
		OpString search_term;

		if(m_search_suggest_in_progress)
		{
			// already in progress - repost it
			StartSearchSuggest();
			return;
		}
		GetText(search_term);
		if(search_term.HasContent())
		{
			m_search_suggest->Connect(search_term, m_search_guid, SearchTemplate::SEARCH_BAR);
		}
		return;
	}
	else if(msg == MSG_QUICK_REMOVE_SEARCH_HISTORY_ITEM)
	{
		SearchDropDownModelItem *item = reinterpret_cast<SearchDropDownModelItem *>(par1);
		SearchDropDownModel* model = GetModel();
		if(item && model && model->GetIndexByItem(item) > -1 && item->IsSearchHistoryItem())
		{
			SearchHistoryModelItem *search_item = static_cast<SearchHistoryModelItem *>(item);

			if(g_search_field_history->RemoveEntry(search_item->GetSearchID(), search_item->GetSearchTerm()))
			{
				item->Delete();
				g_search_field_history->Write();
			}
			if(!HasSearchHistoryItems())
			{
				RemoveSearchHistoryHeader();
			}
			SetSelectedItem(NULL, FALSE);	// don't select any item
		}
		ResizeDropdown();
		return;
	}
	return OpTreeViewDropdown::HandleCallback(msg, par1, par2);
}

OP_STATUS OpSearchDropDown::AddManageSearchEngines()
{
	OpString manage_text;
	g_languageManager->GetString(Str::M_MANAGE_SEARCH_ENGINES, manage_text);

	SearchExecuteActionItem* button_item = OP_NEW(SearchExecuteActionItem, (manage_text.CStr(), OpInputAction::ACTION_MANAGE_SEARCH_ENGINES));
	if(!button_item)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	GetModel()->AddLast(button_item);

	return OpStatus::OK;
}

BOOL OpSearchDropDown::IsValidSearchForList(SearchTemplate* search)
{
	// it needs a valid key (for now)
	if(search->GetKey().IsEmpty())
	{
		return FALSE;
	}
	SearchType type = search->GetSearchType();

	// filter out inline search and history search - see DSK-275869
	if(type == SEARCH_TYPE_INPAGE || type == SEARCH_TYPE_HISTORY)
	{
		return FALSE;
	}
	return TRUE;
}

OP_STATUS OpSearchDropDown::AddSearchEngines(const OpString& search_text)
{
	if (m_is_inside_speeddial)
		return OpStatus::OK; // do nothing.
	SearchDropDownModel* model = GetModel();
	// add the header for the search engines as we need it as a parent when adding the search engines
	SearchParentLabelModelItem* parent_item = OP_NEW(SearchParentLabelModelItem, ());
	if(!parent_item)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	OpString text;
	g_languageManager->GetString(Str::S_SEARCH_FIELD_SEARCH_HEADER, text);

	parent_item->SetText(text.CStr());
	parent_item->SetSkinImage("Search Engines");

	model->AddLast(parent_item);

	// Add searchengines that have a key (if not, don't show it in the list)..
	// Keep in sync with ValidateSearchType()
	for(OP_MEMORY_VAR int i = 0; i < (int) g_searchEngineManager->GetSearchEnginesCount(); i++)
	{
		SearchTemplate* search = g_searchEngineManager->GetSearchEngine(i);

		if (IsValidSearchForList(search))
		{
			SearchDropDownModelItem* item = OP_NEW(SearchDropDownModelItem, ());
			RETURN_OOM_IF_NULL(item);
			item->SetSearchItem(search);
			parent_item->AddChildLast(item);
			if (search->GetUniqueGUID().Compare(m_search_guid) == 0)
			{
				item->SetActiveSearch(TRUE, search_text.CStr());
				m_web_search_item = item;
			}
		}
	}
	return OpStatus::OK;
}

OP_STATUS OpSearchDropDown::UpdateActiveSearchLine(SearchTemplate* new_search, const OpString& search_text)
{
	return OpStatus::OK;
}

BOOL OpSearchDropDown::HasSearchHistoryItems()
{
	INT32 index;

	SearchDropDownModel* model = GetModel();
	for(index = 0; index < model->GetCount(); index++)
	{
		const SearchDropDownModelItem *item = model->GetItemByIndex(index);
		if(item && item->IsSearchHistoryItem())
		{
			return TRUE;
		}
	}
	return FALSE;
}

void OpSearchDropDown::RemoveSearchHistoryHeader()
{
	INT32 index;
	SearchDropDownModel* model = GetModel();
	for(index = 0; index < model->GetCount(); index++)
	{
		SearchDropDownModelItem *item = model->GetItemByIndex(index);
		if(item && item->IsSearchHistoryHeader())
		{
			item->Delete();
			break;
		}
	}
}

OP_STATUS OpSearchDropDown::AddSearchHistoryItems(OpString& filter)
{
	// remove old entries that might be in the list
	INT32 index;
	INT32 insert_index = -1;
	SearchDropDownModel* model = GetModel();
	
	for(index = 0; index < model->GetCount(); index++)
	{
		SearchDropDownModelItem *item = model->GetItemByIndex(index);
		if(item->IsSearchHistoryHeader())
		{
			// also removes all child items
			item->Delete();

			if(insert_index == -1)
			{
				insert_index = index - 1;
			}
			break;
		}
		else if(item->IsSuggestion())
		{
			insert_index = index;
		}
	}
	// add the search field history items
	UINT32 count = 3;
	OpAutoPtr<OpSearchFieldHistory::Iterator> iterator(g_search_field_history->GetIterator());

	// WARNNOTE (ismailp): ResizeDropdown calls may be
	// required before each return statement.
	if(filter.HasContent())
	{
		iterator->SetFilter(filter);
	}
	else
	{
		ResizeDropdown();
		return OpStatus::OK;
	}

	OpSearchFieldHistory::OpSearchFieldHistoryEntry* history_entry = iterator->GetNext();
	if(history_entry)
	{
		// add the header for the search engines as we need it as a parent when adding the search history
		SearchParentLabelModelItem* label_item = OP_NEW(SearchParentLabelModelItem, ());
		if(!label_item)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		OpString text;

		g_languageManager->GetString(Str::S_SEARCH_FIELD_PREVIOUS_SEARCHES, text);

		label_item->SetText(text.CStr());
		label_item->SetSkinImage("Search History");
		label_item->SetIsSearchHistoryHeader(TRUE);
		if(insert_index == -1 && model->GetCount())
			insert_index = model->AddFirst(label_item);
		else
			insert_index = model->InsertAfter(label_item, insert_index);

		while(history_entry && count > 0)
		{
			// not our search engine, skip it
			if(m_is_inside_speeddial && history_entry->GetID() && m_search_guid.Compare(history_entry->GetID()))
			{
				history_entry = iterator->GetNext();
				continue;
			}
			SearchHistoryModelItem *item = OP_NEW(SearchHistoryModelItem, ());
			RETURN_OOM_IF_NULL(item);

			item->SetSearchHistory(history_entry->GetID(), history_entry->GetSearchTerm());

			label_item->AddChildLast(item);

			count--;
			history_entry = iterator->GetNext();
		}
		if (!label_item->GetChildCount()) // fix for DSK-317765 - this is somewhat weird fix, butvery short
			label_item->Delete();
	}
	ResizeDropdown();

	return OpStatus::OK;
}

void OpSearchDropDown::ResizeDropdown()
{
	const SearchDropDownModel* model = GetModel();
	if(model)
	{
		SetMaxLines(model->GetCount());
	}
	OpTreeViewDropdown::ResizeDropdown();
}

void OpSearchDropDown::OnQueryComplete(const OpStringC& search_id, OpVector<SearchSuggestEntry>& entries)
{
	m_search_suggest_in_progress = FALSE;

	m_query_error_counter = 0;
	SearchDropDownModel* model = GetModel();
	if(!model || !DropdownIsOpen())
		return;
	INT32 search_history_index = -1;
	for (SearchDropDownModelItem *item = model->GetItemByIndex(0);
		 NULL != item && item->IsSuggestion();)
	{
		item->Delete();
		item = model->GetItemByIndex(0);
	}


	SearchDropDownModelItem* item = model->GetItemByIndex(0);

	UINT32 cnt;
	OpString filter;
	GetText(filter);
	const unsigned int max_items = m_is_inside_speeddial ? MAX_SUGGESTIONS_SPEEDDIAL_SEARCH : MAX_SUGGESTIONS_SEARCH_DROPDOWN;

	UINT32 max_entries = min(max_items, entries.GetCount());
	for(cnt = 0; cnt < max_entries; ++cnt)
	{
		SearchSuggestEntry *entry = entries.Get(cnt);
		if(!entry)
			continue;
		SearchSuggestionModelItem* suggest_item = OP_NEW(SearchSuggestionModelItem, ());
		if(suggest_item)
		{
			suggest_item->SetSearchSuggestion(entry->GetData());
			suggest_item->SetSearchProvider(m_search_guid.CStr());
			suggest_item->SetIsSuggestion(TRUE);
			// insert items _before_ the first item (whatever that item could be)
			item->InsertSiblingBefore(suggest_item);
			++search_history_index;
		}
	}
	EnsureSeparator();
	ResizeDropdown();
}

UINT32 OpSearchDropDown::GetColumnWeight(UINT32 column)
{
	UINT32 weight;

	switch(column)
	{
		case 0:
			weight = 100;
			break;

		default:
			weight = OpTreeViewDropdown::GetColumnWeight(column);
			break;
	}
	return weight;
}

void OpSearchDropDown::AdjustSize(OpRect& dropdown_rect)
{
	OpWidget* parent = GetParent();
	if(parent && parent->GetType() == OpTypedObject::WIDGET_TYPE_SEARCH_SPECIAL_DROPDOWN)
	{
		const OpRect r = parent->GetRect();
		dropdown_rect.x -= rect.x;
		dropdown_rect.width = r.width + dropdown_rect.width - rect.width;
	}
}

SearchDropDownModelItem *OpSearchDropDown::GetActiveSearchEngineItem()
{
	if (!DropdownIsOpen())
	{
		m_active_search_item = NULL;
	}

	return GetModel() ? m_active_search_item : NULL;
}

OP_STATUS OpSearchDropDown::ExecuteSearchForItem(SearchDropDownModelItem *item)
{
	if(item)
	{
		m_active_search_item = item;
		SearchTemplate *search_template = item->GetSearchItem();
		if(search_template)
		{
			m_follow_default_search = FALSE;
			m_search_guid.Set(search_template->GetUniqueGUID());
			m_search_changed = TRUE;
			UpdateSearchField(search_template);

			SearchEngineManager::SearchSetting settings;
			settings.m_search_template = search_template;

			Search(settings, FALSE);

			// Hide dropdown in case there was no text that started a search (DSK-296552)
			ShowDropdown(FALSE);
		}
		else
		{
			if(item->GetType() == OpTypedObject::WIDGET_TYPE_SEARCH_SUGGESTION_ITEM)
			{
				// user clicked a search suggestion entry
				SearchSuggestionModelItem *search_item = static_cast<SearchSuggestionModelItem *>(item);

				m_follow_default_search = FALSE;
				m_search_changed = TRUE;
				UpdateSearchField();
				SetText(search_item->GetSearchSuggestion());

				SearchEngineManager::SearchSetting settings;
				settings.m_keyword.Set(search_item->GetSearchSuggestion());
				settings.m_search_template = search_template;
				Search(settings, TRUE);
			}
			else if(item->GetType() == OpTypedObject::WIDGET_TYPE_SEARCH_HISTORY_ITEM)
			{
				SearchHistoryModelItem *search_item = static_cast<SearchHistoryModelItem *>(item);
				SearchTemplate* search_template = g_searchEngineManager->GetByUniqueGUID(search_item->GetSearchID());

				m_follow_default_search = FALSE;
				m_search_guid.Set(search_item->GetSearchID());
				m_search_changed = TRUE;
				UpdateSearchField(search_template);

				SetText(search_item->GetSearchTerm());

				SearchEngineManager::SearchSetting settings;
				settings.m_keyword.Set(search_item->GetSearchTerm());
				settings.m_search_template = search_template;
				Search(settings, FALSE);
			}
			else if(item->GetType() == OpTypedObject::WIDGET_TYPE_SEARCH_ACTION_ITEM)
			{
				SearchExecuteActionItem* search_item = static_cast<SearchExecuteActionItem *>(item);
				g_input_manager->InvokeAction(search_item->GetAction(), 0, 0);
				m_active_search_item = NULL;
			}
		}
	}
	return OpStatus::OK;
}

void OpSearchDropDown::OnDesktopWindowClosing(DesktopWindow* desktop_window,
												BOOL user_initiated)
{
	// Both 2 base classes listen to DesktopWindows, but not the same one
	// don't forward message for one to another
	if (desktop_window->GetType() == WINDOW_TYPE_DOCUMENT)
	{
		DocumentDesktopWindowSpy::OnDesktopWindowClosing(desktop_window, user_initiated);
	}
	else
	{
		if(GetParent() && GetParent()->GetType() == WIDGET_TYPE_SEARCH_SPECIAL_DROPDOWN)
		{
			OpButton* dropdown_button = ((OpSearchDropDownSpecial *)GetParent())->GetDropdownButton();
			if (dropdown_button)
			{
				dropdown_button->OnMouseLeave();
				// OpInputAction::ACTION_SHOW_POPUP_MENU is doing child_action->SetSelected so we
				// must unset that pushed state that resulted in so the button looks released again.
				dropdown_button->SetValue(0);
			}
		}
		OpTreeViewDropdown::OnDesktopWindowClosing(desktop_window, user_initiated);
	}
}

void SearchDropDownModelItem::SetActiveSearch(BOOL active, const uni_char* text) 
{
	m_active_search = active; 
	m_active_search_text.Set(text); 
	Change();
}

/************************************************************************************************
*
* An item in the SearchDropDownModel used to populate the search drop down treeview. This class
* represents a regular search engine entry.
*
************************************************************************************************/
OP_STATUS SearchDropDownModelItem::GetItemData(OpTreeModelItem::ItemData *item_data)
{
	if (item_data->query_type == INIT_QUERY)
	{
		if (IsSeparator())
		{
			item_data->flags |= FLAG_SEPARATOR;
			item_data->flags |= FLAG_NO_SELECT;
		}
		else
		{
			// TODO: Make so this is supported on COLUMN_QUERY too!
			item_data->flags |= FLAG_FORMATTED_TEXT;
		}
	}
	else if (item_data->query_type  == SKIN_QUERY)
	{
		item_data->skin_query_data.skin = "Address Bar Drop Down Item";
	}
	else if (item_data->query_type == COLUMN_QUERY && m_search)
	{
		RETURN_IF_ERROR(m_search->GetItemData(item_data));
		item_data->column_query_data.column_indentation = SEARCH_COLUMN_INDENTATION;
	}
	return OpStatus::OK;
}


/************************************************************************************************
*
* An item in the SearchDropDownModel used to populate the search suggestion list.
*
************************************************************************************************/

OP_STATUS SearchSuggestionModelItem::GetItemData(OpTreeModelItem::ItemData *item_data)
{
	if(item_data->query_type == COLUMN_QUERY)
	{
		if (item_data->column_query_data.column == 0 && item_data->flags & FLAG_FOCUSED)
		{
			item_data->column_query_data.column_image = "Address Dropdown Search Icon Focused";
		}
		
		if(item_data->column_query_data.column == 1)
		{
			item_data->column_query_data.column_indentation = SEARCH_COLUMN_INDENTATION;
			OpString result;
			RETURN_IF_ERROR(result.Set(m_search_suggestion));
			if (!(item_data->flags & FLAG_FOCUSED))
			{
				
				int color = SkinUtils::GetTextColor("Address DropDown URL Label");
				if (color >= 0)
				{
					RETURN_IF_ERROR(StringUtils::AppendColor(result, color, 0, result.Length()));
				}
			}
			item_data->column_query_data.column_text->Set(result.CStr());
		}
		return OpStatus::OK;
	}
	return SearchDropDownModelItem::GetItemData(item_data);
}

/************************************************************************************************
*
* An item in the SearchDropDownModel used to populate the search drop down treeview. This class
* represents a custom item used as top level parent items the treeview.
*
************************************************************************************************/

OP_STATUS SearchParentLabelModelItem::GetItemData(OpTreeModelItem::ItemData *item_data)
{
	switch(item_data->query_type)
	{
		case INIT_QUERY:
		{
			item_data->flags |= FLAG_INITIALLY_OPEN;

			if (!IsSelectable())
				item_data->flags |= FLAG_NO_SELECT;
			
			break;
		}
		case  SKIN_QUERY:
		{
			item_data->skin_query_data.skin = "Search Treeview Header Item Skin";

			break;
		}
		case COLUMN_QUERY:
		{
			if (item_data->column_query_data.column == 1)
			{
				item_data->column_query_data.column_text->Set(GetText());
				
				OpStatus::Ignore(g_skin_manager->GetTextColor("Search Treeview Header Item Skin", &item_data->column_query_data.column_text_color));
			}
			else if (item_data->column_query_data.column == 0)
			{
				item_data->column_query_data.column_image = m_skin_image.CStr();
			}
			item_data->column_query_data.column_indentation = SEARCH_COLUMN_INDENTATION;

			break;
		}
	}
	return OpStatus::OK;
}

/************************************************************************************************
*
* An item in the SearchDropDownModel used to populate the search drop down treeview. This class
* represents a custom item with information about previous searches.
*
************************************************************************************************/
OP_STATUS SearchHistoryModelItem::GetItemData(OpTreeModelItem::ItemData *item_data)
{
	switch(item_data->query_type)
	{
		case INIT_QUERY:
		{
			item_data->flags |= FLAG_FORMATTED_TEXT;

			if (!IsSelectable())
				item_data->flags |= FLAG_NO_SELECT;

			return OpStatus::OK;
		}

		case SKIN_QUERY:
		{
			SearchDropDownModelItem::GetItemData(item_data);
			return OpStatus::OK;
		}

		case ASSOCIATED_IMAGE_QUERY:
		{
			RETURN_IF_ERROR(item_data->associated_image_query_data.image->Set("Search Close History Item"));
			return OpStatus::OK;
		}

		case COLUMN_QUERY:
		{
			if(item_data->column_query_data.column == 0)
			{
				item_data->column_query_data.column_indentation = SEARCH_COLUMN_INDENTATION;
				SearchTemplate* search_template = g_searchEngineManager->GetByUniqueGUID(m_search_id);
				if(search_template)
				{
					Image image = search_template->GetIcon();

					if(image.IsEmpty())
					{
						item_data->column_query_data.column_image = "Search Web";
					}
					else
					{
						item_data->column_bitmap = image;
					}
				}
				return OpStatus::OK;
			}
			else if(item_data->column_query_data.column == 1)
			{
				SearchTemplate* search_template = g_searchEngineManager->GetByUniqueGUID(m_search_id);
				if(search_template)
				{
					item_data->column_query_data.column_text->Set(m_search_term.CStr());
				}
				return OpStatus::OK;
			}
		}
	}
	return SearchDropDownModelItem::GetItemData(item_data);
}

/************************************************************************************************
*
* An item in the SearchDropDownModel used to populate the search drop down treeview. This class
* represents a custom item with an associated action to be executed when the item is selected.
*
************************************************************************************************/

OP_STATUS SearchExecuteActionItem::GetItemData(OpTreeModelItem::ItemData *item_data)
{
	switch(item_data->query_type)
	{
		case SKIN_QUERY:
		{
			item_data->skin_query_data.skin = "Search Treeview Manage Searches Item Skin";
			return OpStatus::OK;
		}

		case COLUMN_QUERY:
		{
			if(item_data->column_query_data.column == 0)
			{
				item_data->column_query_data.column_image = "Search Settings";
			}
			if(item_data->column_query_data.column == 1)
			{
				item_data->column_query_data.column_text->Set(m_title.CStr());
			}
			item_data->column_query_data.column_indentation = SEARCH_COLUMN_INDENTATION;
			return OpStatus::OK;
		}
	}
	return SearchDropDownModelItem::GetItemData(item_data);
}

/***********************************************************************************
**
**	OpSearchDropDownSpecial
**
** Wraps the regular OpSearchDropDown with a custom button to activate the dropdown
**
**
***********************************************************************************/

DEFINE_CONSTRUCT(OpSearchDropDownSpecial)

OpSearchDropDownSpecial::OpSearchDropDownSpecial() :
m_search_drop_down(NULL)
, m_dropdown_button(NULL)
#ifdef QUICK_SEARCH_DROPDOWN_MAC_STYLE
, m_clear_button(NULL)
#else
, m_go_button(NULL)
#endif // QUICK_SEARCH_DROPDOWN_MAC_STYLE
{
	init_status = OpButton::Construct(&m_dropdown_button, OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE);
	RETURN_VOID_IF_ERROR(init_status);
	
	m_dropdown_button->GetBorderSkin()->SetImage("Dropdown Search Field Button Skin");
	m_dropdown_button->GetForegroundSkin()->SetImage("Search Button Image");
	m_dropdown_button->SetRestrictImageSize(FALSE);
	m_dropdown_button->SetFixedTypeAndStyle(TRUE);
	
	OpInputAction *action = OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_POPUP_MENU));
	
	if(!action)
	{
		init_status = OpStatus::ERR_NO_MEMORY;
		return;
	}
	m_dropdown_button->SetAction(action);
	m_dropdown_button->SetName(WIDGET_NAME_MAINSEARCH_BUTTON_DROPDOWN);

	AddChild(m_dropdown_button, TRUE);

	init_status = OpSearchDropDown::Construct(&m_search_drop_down);
	RETURN_VOID_IF_ERROR(init_status);

	m_search_drop_down->ShowButton(FALSE);
	m_search_drop_down->GetBorderSkin()->SetImage("Dropdown Search Field Skin", "Dropdown Search Skin");

	AddChild(m_search_drop_down, TRUE);

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	AccessibilitySkipsMe();
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

#ifdef QUICK_SEARCH_DROPDOWN_MAC_STYLE

	init_status = MacOpClearButton::Construct(&m_clear_button, OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE);
	RETURN_VOID_IF_ERROR(init_status);

	m_clear_button->GetBorderSkin()->SetImage("Dropdown Search Field Close Button Skin");
	m_clear_button->SetRestrictImageSize(FALSE);
	m_clear_button->SetFixedTypeAndStyle(TRUE);
	m_clear_button->SetListener(this);

	UpdateCloseIcon();

	AddChild(m_clear_button, TRUE);

#else
	init_status = OpButton::Construct(&m_go_button, OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE);
	RETURN_VOID_IF_ERROR(init_status);

	m_go_button->GetBorderSkin()->SetImage("Dropdown Search Field Go Button Skin");
	m_go_button->GetForegroundSkin()->SetImage("Search Go Button Image");
	m_go_button->SetRestrictImageSize(FALSE);
	m_go_button->SetFixedTypeAndStyle(TRUE);
	m_go_button->SetListener(this);

	m_go_button->SetName(WIDGET_NAME_MAINSEARCH_BUTTON_GO);

	action = OP_NEW(OpInputAction, (OpInputAction::ACTION_SEARCH_FIELD_GO));
	if(!action)
	{
		init_status = OpStatus::ERR_NO_MEMORY;
		return;
	}
	m_go_button->SetAction(action);

	AddChild(m_go_button, TRUE);
#endif // QUICK_SEARCH_DROPDOWN_MAC_STYLE

	g_favicon_manager->AddListener(this); // Ignore any errors here
}

void OpSearchDropDownSpecial::OnLayout()
{
	OpWidget::OnLayout();

	OpRect bounds = GetBounds();

	INT32 w, h;

	m_dropdown_button->GetPreferedSize(&w, &h, 1, 1);

	OpRect edit_rect = bounds;
	OpRect btn_rect;

	btn_rect.height = max(h, bounds.height);
	btn_rect.width = w;
	btn_rect.x = bounds.x;
	btn_rect.y = bounds.y;

	edit_rect.x += w;
	edit_rect.width -= w;

#ifdef QUICK_SEARCH_DROPDOWN_MAC_STYLE

	m_clear_button->GetPreferedSize(&w, &h, 1, 1);

	OpRect clear_rect;

	clear_rect.height = max(h, bounds.height);
	clear_rect.width = w;
	clear_rect.x = bounds.width - bounds.x - w;
	clear_rect.y = bounds.y;

	edit_rect.width -= w;

	SetChildRect(m_clear_button, clear_rect);

	UpdateCloseIcon();
#else
	m_go_button->GetPreferedSize(&w, &h, 1, 1);

	OpRect go_rect;

	go_rect.height = max(h, bounds.height);
	go_rect.width = w;
	go_rect.x = bounds.width - bounds.x - w;
	go_rect.y = bounds.y;

	edit_rect.width -= w;

	SetChildRect(m_go_button, go_rect);

#endif // QUICK_SEARCH_DROPDOWN_MAC_STYLE

	SetChildRect(m_search_drop_down, edit_rect);
	SetChildRect(m_dropdown_button, btn_rect);
	
}

BOOL OpSearchDropDownSpecial::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_SHOW_POPUP_MENU:
					{
						child_action->SetEnabled(TRUE);
						child_action->SetSelected(m_search_drop_down->DropdownIsOpen());
					}
					return TRUE;

				case OpInputAction::ACTION_SEARCH_FIELD_GO:
					{
						if(m_search_drop_down)
						{
							OpString text;

							m_search_drop_down->GetText(text);

							child_action->SetEnabled(text.HasContent());
						}
					}
					return TRUE;
			}
			return FALSE;
		}

		case OpInputAction::ACTION_SHOW_POPUP_MENU:
		case OpInputAction::ACTION_SHOW_SEARCH_DROPDOWN:
		{
			if (!m_search_drop_down->DropdownIsOpen())
			{
				// focus edit field so keyboard handling will work
				m_search_drop_down->edit->SetFocus(FOCUS_REASON_MOUSE);
				m_search_drop_down->ShowMenu();
			}
			else
			{
				m_search_drop_down->ShowDropdown(FALSE);
			}
			return TRUE;
		}
		case OpInputAction::ACTION_CLEAR_SEARCH_FIELD:
		{
			if(m_search_drop_down)
			{
				m_search_drop_down->ResetQueryErrorCounter();
				m_search_drop_down->SetText(NULL);
				m_search_drop_down->SetFocus(FOCUS_REASON_MOUSE);
			}
			return TRUE;
		}
		break;

		case OpInputAction::ACTION_SEARCH_FIELD_GO:
		{
			if (m_search_drop_down)
			{
				SearchEngineManager::SearchSetting settings;
				m_search_drop_down->Search(settings, FALSE);
			}
			return TRUE;
		}

		case OpInputAction::ACTION_CLOSE_DROPDOWN:
		{
			if (m_search_drop_down->DropdownIsOpen())
			{
				m_search_drop_down->ShowDropdown(FALSE);
				return TRUE; // Eat action only if we use it
			}
		}
	}
	return OpWidget::OnInputAction(action);
}

void OpSearchDropDownSpecial::OnFavIconAdded(const uni_char* document_url, const uni_char* image_path)
{
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
	SearchTemplate* search = g_searchEngineManager->GetByUniqueGUID(m_search_drop_down->GetSearchGUID());
	
	if (search)
	{
		if(document_url && *document_url && uni_strstr(search->GetUrl().CStr(), document_url))
		{
			Image img = g_favicon_manager->Get(search->GetUrl().CStr());

			if(!img.IsEmpty())
				m_dropdown_button->GetForegroundSkin()->SetBitmapImage(img, FALSE);
			else
				m_dropdown_button->GetForegroundSkin()->SetImage(search->GetSkinIcon());
		}
	}
#endif // DESKTOP_UTIL_SEARCH_ENGINES
}

void OpSearchDropDownSpecial::OnFavIconsRemoved()
{
	UpdateSearchIcon();
}

void OpSearchDropDownSpecial::UpdateSearchIcon()
{
	SearchTemplate* search = g_searchEngineManager->GetByUniqueGUID(m_search_drop_down->GetSearchGUID());
	if (search)
	{
		Image img = search->GetIcon();

		if(!img.IsEmpty())
			m_dropdown_button->GetForegroundSkin()->SetBitmapImage(img, FALSE);
		else
			m_dropdown_button->GetForegroundSkin()->SetImage(search->GetSkinIcon());
	}
}

void OpSearchDropDownSpecial::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	if(m_search_drop_down)
	{
		m_search_drop_down->GetPreferedSize(w, h, cols, rows);
	}
}

void OpSearchDropDownSpecial::OnDeleted()
{
	if( g_favicon_manager )
	{
		g_favicon_manager->RemoveListener(this);
	}
	OpWidget::OnDeleted();
}

void OpSearchDropDownSpecial::OnContentSizeChanged()
{ 
#ifdef QUICK_SEARCH_DROPDOWN_MAC_STYLE
	UpdateCloseIcon();
#else
	g_input_manager->UpdateAllInputStates(FALSE);
#endif // QUICK_SEARCH_DROPDOWN_MAC_STYLE
}
#ifdef QUICK_SEARCH_DROPDOWN_MAC_STYLE

void OpSearchDropDownSpecial::UpdateCloseIcon()
{
	OpString text;
	
	if(m_search_drop_down && OpStatus::IsSuccess(m_search_drop_down->GetText(text)))
	{
		if(text.HasContent())
		{
			m_clear_button->GetForegroundSkin()->SetImage("Search Close Image");
			OpInputAction *action = OP_NEW(OpInputAction, (OpInputAction::ACTION_CLEAR_SEARCH_FIELD));
			if(action)
				m_clear_button->SetAction(action);
		}
		else
		{
			m_clear_button->GetForegroundSkin()->SetImage(NULL);
			m_clear_button->SetAction(NULL);
		}
	}
}

OP_STATUS MacOpClearButton::Construct(MacOpClearButton** obj, ButtonType button_type, ButtonStyle button_style)
{
	RETURN_VALUE_IF_NULL(obj, OpStatus::ERR_NULL_POINTER);
	*obj = OP_NEW(MacOpClearButton, (button_type, button_style));
	RETURN_OOM_IF_NULL(*obj);
	
	if (OpStatus::IsError((*obj)->init_status))
	{
		OP_DELETE(*obj);
		*obj = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

void MacOpClearButton::OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks) {
	OpButton::OnMouseDown(point, button, nclicks); // do the default
	static_cast<OpSearchDropDownSpecial*>(GetParent())->GetSearchInput()->SetFocus(FOCUS_REASON_MOUSE);  // set focus to the edit box.
}

#endif // QUICK_SEARCH_DROPDOWN_MAC_STYLE

BOOL OpSearchDropDown::HasToolTipText(OpToolTip* tooltip)
{
	return TRUE;
}

void OpSearchDropDown::GetToolTipText(OpToolTip* tooltip, OpInfoText& text)
{
	text.SetTooltipText(m_ghost_string);
}

void OpSearchDropDown::UpdateFollowDefaultSearch()
{
	if (m_follow_default_search)
	{
		// if not following speed dial search and not special search type
		if (!m_is_inside_speeddial && m_type == SEARCH_TYPE_GOOGLE)
		{
			// and still using default search
			SearchTemplate* default_search = g_searchEngineManager->GetDefaultSearch();
			if (default_search && default_search->GetUniqueGUID() == m_search_guid)
			{
				return; // leave it TRUE
			}
		}
		m_follow_default_search = FALSE;
	}
}

void OpSearchDropDown::OnSettingsChanged(DesktopSettings* settings)
{
	if (settings->IsChanged(SETTINGS_SEARCH))
	{
		if (m_is_inside_speeddial ||
			m_follow_default_search || // DSK-305258
			m_search_guid.IsEmpty())   // can happen if searches are loaded after country check (DSK-351304)
		{
			SearchTemplate* search;
			if (m_is_inside_speeddial)
			{
				search = g_searchEngineManager->GetDefaultSpeedDialSearch();
			}
			else
			{
				search = g_searchEngineManager->GetDefaultSearch();
			}
			if (search)
			{
				m_search_guid.Set(search->GetUniqueGUID());
			}
			else
			{
				m_search_guid.Empty();
			}
			m_search_changed = TRUE;
			ValidateSearchType();
			UpdateSearchField();
		}
	}
}
