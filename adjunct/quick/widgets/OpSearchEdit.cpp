/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "OpSearchEdit.h"

#include "modules/doc/doc.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/pi/OpDragManager.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/dragdrop/dragdrop_manager.h"

#include "adjunct/desktop_util/search/search_net.h"
#include "adjunct/desktop_util/search/searchenginemanager.h"
#include "adjunct/desktop_util/actions/action_utils.h"
#include "adjunct/quick/managers/FavIconManager.h"
#include "adjunct/quick_toolkit/widgets/OpToolbar.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick/Application.h"

#ifdef ENABLE_USAGE_REPORT
#include "adjunct/quick/usagereport/UsageReport.h"
#endif

DEFINE_CONSTRUCT(OpSearchEdit);

/***********************************************************************************
 **
 **	OpSearchEdit
 **
 ** @param search_type - this really is index in searchengines model 
 **                      (and should not be used in this way)
 **                      Default value is 0
 **
 **                      However in some cases it is a search type, 
 **                      then it's -type, so to find the correct search,
 **                      take abs(type) and find the index of the search
 **                      with the given type
 **
 ***********************************************************************************/
// Yes, sorry about that param name, but honestly, that's what it is
OpSearchEdit::OpSearchEdit(INT32 index_or_negated_search_type)
	: m_search_in_page(FALSE)
	, m_search_in_speeddial(FALSE)
	, m_status(SEARCH_STATUS_NORMAL)
{
	Initialize(index_or_negated_search_type);
}

OpSearchEdit::OpSearchEdit(const OpString& guid)
	: m_search_in_page(FALSE)
	, m_search_in_speeddial(FALSE)
	, m_status(SEARCH_STATUS_NORMAL)
{
	SearchTemplate* search = g_searchEngineManager->GetByUniqueGUID(guid);
	Initialize((search ? g_searchEngineManager->GetSearchIndex(search) : 0));
}

void OpSearchEdit::Initialize(INT32 index_or_negated_search_type)
{
	GetBorderSkin()->SetImage("Edit Search Skin", "Edit Skin");
	SetSpyInputContext(this, FALSE);
	SetUpdateNeededWhen(UPDATE_NEEDED_WHEN_VISIBLE);

	SetListener(this);

	g_favicon_manager->AddListener(this);

	UpdateSearch(index_or_negated_search_type);

	OpInputAction* action = OP_NEW(OpInputAction, (OpInputAction::ACTION_SEARCH));
	if (!action)
	{
		init_status = OpStatus::ERR_NO_MEMORY;
		return;
	}
	action->SetActionData(index_or_negated_search_type);

	SetAction(action);
}

/***********************************************************************************/

void OpSearchEdit::SetSearchStatus(SEARCH_STATUS status)
{
	m_status = status;
	InvalidateAll();
}

void OpSearchEdit::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	if (m_status == SEARCH_STATUS_NOMATCH)
		g_skin_manager->DrawElement(vis_dev, "Edit Search No Match Skin", GetBounds());

	OpEdit::OnPaint(widget_painter, paint_rect);
}

/***********************************************************************************
 **
 ** OnDeleted
 **
 ***********************************************************************************/

void OpSearchEdit::OnDeleted()
{
	SetSpyInputContext(NULL, FALSE);

	if (g_favicon_manager )
	{
		g_favicon_manager->RemoveListener(this);
	}

	OpEdit::OnDeleted();
}

/***********************************************************************************
 **
 ** 
 **
***********************************************************************************/
void OpSearchEdit::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (m_search_in_page)
	{
		Search(0);
	}
}

/***********************************************************************************
 **
 ** UpdateSearch
 **
 ***********************************************************************************/
void OpSearchEdit::UpdateSearch(INT32 index_or_negated_search_type)
{
	m_index_or_negated_search_type = index_or_negated_search_type;
	m_index_in_search_model        = TranslateType(m_index_or_negated_search_type);

	SearchTemplate* search = g_searchEngineManager->GetSearchEngine(m_index_in_search_model);

	// This is actually the search type
	INT32 type = search ? search->GetSearchType() : -1;

	m_search_in_page = (type == SEARCH_TYPE_INPAGE);
	autocomp.SetType(type == SEARCH_TYPE_GOOGLE ? AUTOCOMPLETION_GOOGLE : AUTOCOMPLETION_OFF);

	SetOnChangeOnEnter(!m_search_in_page);

	// Set the action that should be performed when enter is pressed currently
	// the index_or_type is used in the action to identify the search
	OpInputAction* action = OP_NEW(OpInputAction, (m_search_in_page ? OpInputAction::ACTION_FIND_NEXT : OpInputAction::ACTION_SEARCH));
	if (!action)
		return;
	action->SetActionData(m_search_in_page ? TRUE : index_or_negated_search_type);
	SetAction(action);

	// Update ghost string
	OpString buf;
	g_searchEngineManager->MakeSearchWithString(m_index_in_search_model, buf);
	SetGhostText(buf.HasContent() ? buf.CStr() : UNI_L(""));

	GetForegroundSkin()->SetImage("Search Web");
	UpdateSearchIcon();

}

/***********************************************************************************
 **
 ** SetForceSearchInpage - force inline search, typically used for the search bar
 **
 ***********************************************************************************/
void OpSearchEdit::SetForceSearchInpage(BOOL force_search_in_page)
{ 
	m_search_in_page = force_search_in_page; 

	SetOnChangeOnEnter(!m_search_in_page);

	// Set the action that should be performed when enter is pressed currently
	// the index_or_type is used in the action to identify the search
	OpInputAction* action = OP_NEW(OpInputAction, (m_search_in_page ? OpInputAction::ACTION_FIND_NEXT : OpInputAction::ACTION_SEARCH));
	if (!action)
		return;
	action->SetActionData(m_search_in_page ? TRUE : m_index_or_negated_search_type);
	SetAction(action);

	// workaround for missing inline search in configured searches
	if(m_search_in_page && m_index_in_search_model == 0)
	{
		// Update ghost string
		OpString buf;
		TRAPD(err, buf.ReserveL(64));

		g_languageManager->GetString(Str::S_FIND_IN_PAGE, buf);

		RemoveChars(buf, UNI_L("&"));

		SetGhostText(buf.CStr());
	}
	UpdateSearchIcon();
}

/***********************************************************************************
 **
 **	TranslateType
 **
 ** @param INT32 index_or_negated_search_type
 **
 ***********************************************************************************/

INT32 OpSearchEdit::TranslateType(INT32 index_or_negated_search_type)
{
	if (index_or_negated_search_type >= 0)
	{
		// It's actually an index already :)
		return index_or_negated_search_type;
	}

	// When search_type is negative is is the only time it is really a search
	// so make it posivitve then search for the search with that type and
	// return its index
	INT32 search_type = abs(index_or_negated_search_type);
	return g_searchEngineManager->SearchTypeToIndex((SearchType)search_type);
}

/***********************************************************************************
 **
 **	UpdateSearchIcon
 **
 ***********************************************************************************/

void OpSearchEdit::UpdateSearchIcon()
{
	if (m_search_in_page)
	{
		GetForegroundSkin()->SetImage("Search Web");
	}
	else
	{
		SearchTemplate* search = g_searchEngineManager->GetSearchEngine(m_index_in_search_model);

		if (search)
		{
			Image img = search->GetIcon();

			if (!img.IsEmpty())
				GetForegroundSkin()->SetBitmapImage(img, FALSE);
			else
				GetForegroundSkin()->SetImage(search->GetSkinIcon());
		}
	}
}

/***********************************************************************************
 **
 ** OnFavIconAdded
 **
 ***********************************************************************************/

void OpSearchEdit::OnFavIconAdded(const uni_char* document_url, const uni_char* image_path)
{
	UpdateSearchIcon();
}

/***********************************************************************************
 **
 ** OnFavIconsRemoved
 **
 ***********************************************************************************/

void OpSearchEdit::OnFavIconsRemoved()
{
	UpdateSearchIcon();
}


/***********************************************************************************
 **
 **	Search
 **
 ***********************************************************************************/

void OpSearchEdit::Search(OpInputAction* action)
{
	OpString keyword;
	GetText(keyword);

	if (m_search_in_page)
	{
		g_input_manager->InvokeAction(OpInputAction::ACTION_FIND_INLINE, 0, keyword.CStr());
	}
	else if (action)
	{
		SearchEngineManager::SearchSetting settings;
		g_application->AdjustForAction(settings, action, 0);

		if (m_search_in_speeddial)
		{
			BrowserDesktopWindow* bw = g_application->GetActiveBrowserDesktopWindow();
			if (bw)
				settings.m_target_window = bw->GetActiveDocumentDesktopWindow();
		}
		else
		{
			settings.m_target_window = GetTargetDocumentDesktopWindow();
		}

		settings.m_keyword.Set(keyword);

		int id = g_searchEngineManager->SearchIndexToID(m_index_in_search_model);
		settings.m_search_template = g_searchEngineManager->SearchFromID(id);
		settings.m_search_issuer = m_search_in_speeddial 
			? SearchEngineManager::SEARCH_REQ_ISSUER_SPEEDDIAL : SearchEngineManager::SEARCH_REQ_ISSUER_OTHERS;

		if (settings.m_keyword.HasContent() && settings.m_search_template)
		{
			g_searchEngineManager->DoSearch(settings);

#ifdef ENABLE_USAGE_REPORT
			if(GetParent() && GetParent()->GetType() == WIDGET_TYPE_SPEEDDIAL_SEARCH)
			{
				if(g_usage_report_manager && g_usage_report_manager->GetSearchReport())
				{
					g_usage_report_manager->GetSearchReport()->AddSearch(SearchReport::SearchSpeedDial, settings.m_search_template->GetUniqueGUID());
				}
			}
#endif
		}

	}
}


/***********************************************************************************
 **
 **	OnInputAction
 **
 ***********************************************************************************/

BOOL OpSearchEdit::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		// Just so that we can Escape into the document
		case OpInputAction::ACTION_STOP:
		{
			if (action->IsKeyboardInvoked() && 
				GetTargetDocumentDesktopWindow() && 
				!GetTargetDocumentDesktopWindow()->GetWindowCommander()->IsLoading() && 
				IsFocused())
			{
				g_input_manager->InvokeAction(OpInputAction::ACTION_FOCUS_PAGE);
				return TRUE;
			}
			break;
		}
		case OpInputAction::ACTION_SEARCH:
		{
			UpdateSearch(action->GetActionData());
			Search(action);
			return TRUE;
		}
	}

	return OpEdit::OnInputAction(action);
}

/***********************************************************************************
 **
 **	OnDragStart
 **
 ** Note: the ID set on the drag object is the index_or_negated_search_type
 **
 ***********************************************************************************/

void OpSearchEdit::OnDragStart(OpWidget* widget,INT32 pos, INT32 x, INT32 y)
{
	if (!g_application->IsDragCustomizingAllowed())
		return;

	DesktopDragObject* drag_object = GetDragObject(OpTypedObject::DRAG_TYPE_SEARCH_EDIT, x, y);

	if (drag_object)
	{
		drag_object->SetID(m_index_or_negated_search_type);
		drag_object->SetObject(this);
		g_drag_manager->StartDrag(drag_object, NULL, FALSE);
	}
}

/***********************************************************************************
 **
 ** OnContextMenu
 **
 ***********************************************************************************/

/*virtual*/ BOOL
OpSearchEdit::OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint &menu_point, const OpRect *avoid_rect, BOOL keyboard_invoked)
{
	BOOL handled = static_cast<OpWidgetListener*>(GetParent())->OnContextMenu(this, child_index, menu_point, avoid_rect, keyboard_invoked);
	if (!handled)
	{
		HandleWidgetContextMenu(widget, menu_point);
	}
	return handled;
}

/***********************************************************************************
 **
 ** OnSettingsChanged
 **
 ***********************************************************************************/

void OpSearchEdit::OnSettingsChanged(DesktopSettings* settings)
{
	OpEdit::OnSettingsChanged(settings);

	if (settings->IsChanged(SETTINGS_SEARCH) && m_search_in_speeddial)
	{
		SearchTemplate* search = g_searchEngineManager->GetDefaultSpeedDialSearch();
		// We currently (unfortunately) need to send the index to UpdateSearch, so get it
		UpdateSearch(g_searchEngineManager->GetSearchIndex(search));
	}
}

