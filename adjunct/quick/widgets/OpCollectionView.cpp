/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
*
* Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
*
* This file is part of the Opera web browser.	It may not be distributed
* under any circumstances.
*
* @author Owner:    Karianne Ekern (karie), Deepak Arora (deepaka)
*
*/

#include "core/pch.h"

#include "adjunct/quick/widgets/OpCollectionView.h"

#include "adjunct/quick/models/NavigationModel.h"
#include "adjunct/quick/models/DesktopHistoryModel.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick/widgets/CollectionViewPane.h"
#include "adjunct/quick/widgets/CollectionNavigationPane.h"

#include "modules/skin/OpSkinManager.h"
#include "modules/widgets/WidgetContainer.h"

OpCollectionView::OpCollectionView()
	: m_collection_view_pane(NULL)
	, m_navigation_pane(NULL)
	, m_current_iterator(NULL)
	, m_current_selected_item(NULL)
{
}

OpCollectionView::~OpCollectionView()
{
	OP_DELETE(m_current_iterator);

	if (g_desktop_bookmark_manager)
		g_desktop_bookmark_manager->RemoveBookmarkListener(this);
}

OP_STATUS OpCollectionView::Initialize()
{
	// NavigationPane
	OpAutoPtr<CollectionNavigationPane> navigation_pane(OP_NEW(CollectionNavigationPane, (this)));
	RETURN_OOM_IF_NULL(navigation_pane.get());
	TRAPD(err, navigation_pane->InitL());
	RETURN_IF_ERROR(err);

	// ViewPane
	OpAutoPtr<CollectionViewPane> collectionview_controller(OP_NEW(CollectionViewPane, ()));
	RETURN_OOM_IF_NULL(collectionview_controller.get());
	TRAP(err, collectionview_controller->InitL(this));
	RETURN_IF_ERROR(err);

	m_navigation_pane = navigation_pane.release();
	m_collection_view_pane = collectionview_controller.release();

	AddChild(m_navigation_pane);
	AddChild(m_collection_view_pane, TRUE);

	SetDivision(g_pcui->GetIntegerPref(PrefsCollectionUI::CollectionSplitter));
	SetHorizontal(TRUE);

	if (g_bookmark_manager->IsBusy())
		OpStatus::Ignore(g_desktop_bookmark_manager->AddBookmarkListener(this));
	else
		OpenFolder(m_navigation_pane->GetBookmarkRootNavigationFolder());

	return OpStatus::OK;
}

OP_STATUS OpCollectionView::Create(OpWidget* container)
{
	RETURN_IF_ERROR(Initialize());
	container->AddChild(this);
	m_doc_view_fav_icon.SetImage("Window Collection View Icon");

	return OpStatus::OK;
}

void OpCollectionView::GetThumbnailImage(Image& image)
{
	OpSkinElement* skin_elm = g_skin_manager->GetSkinElement("Bookmarks Panel Window Thumbnail Icon");
	if (skin_elm)
		image = skin_elm->GetImage(0);
}

OP_STATUS OpCollectionView::GetTooltipText(OpInfoText& text)
{
	OpString loc_str;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::SI_IDSTR_HL_TREE_TITLE, loc_str));

	OpString title;
	RETURN_IF_ERROR(GetTitle(title));
	OP_ASSERT(title.HasContent());

	return text.AddTooltipText(loc_str.CStr(), title.CStr());
}

double OpCollectionView::GetWindowScale()
{
	return m_collection_view_pane ? m_collection_view_pane->GetWindowScale() : 1.0;
}

void OpCollectionView::QueryZoomSettings(double &min, double &max, double &step, const OpSlider::TICK_VALUE* &tick_values, UINT8 &num_tick_values)
{
	static const OpSlider::TICK_VALUE tick_values_collection_view[] =
													{ {MIN_ZOOM, FALSE },
													{0.5, TRUE },
													{1  , TRUE },
													{1.5, TRUE },
													{MAX_ZOOM, FALSE } };
	min = MIN_ZOOM;
	max = MAX_ZOOM;
	step = SCALE_DELTA;
	num_tick_values = ARRAY_SIZE(tick_values_collection_view);
	tick_values = tick_values_collection_view;
}

void OpCollectionView::OpenFolder(NavigationItem* folder)
{
	if (m_current_selected_item == folder)
		return;

	m_current_selected_item = folder;
	m_navigation_pane->SelectItem(folder->GetID());

	CollectionModelIterator::SortType sort_type = static_cast<CollectionModelIterator::SortType>(g_pcui->GetIntegerPref(PrefsCollectionUI::CollectionSortType));
	CollectionModelIterator* it = folder->GetItemModel()->CreateModelIterator(folder->GetCollectionModelItem(), sort_type, !!folder->IsRecentFolder());
	if (!it)
		return;

	OP_DELETE(m_current_iterator);
	m_current_iterator = it;
	m_collection_view_pane->ConstructView(it);
}

void OpCollectionView::OpenSelectedFolder(NavigationItem* item)
{
	if (g_hotlist_manager->GetBookmarksModel()->Loaded() && item && item->GetItemModel())
		OpenFolder(item);
}

void OpCollectionView::SortItems(CollectionModelIterator::SortType type)
{
	CollectionModelIterator::SortType sort_type = type;

	OP_ASSERT(m_current_iterator->GetModel());
	CollectionModelIterator* new_it = m_current_iterator->GetModel()->CreateModelIterator(
		m_current_iterator->GetFolder()
		, sort_type
		, !!m_current_iterator->IsRecent());

	if (!new_it)
		return;

	OP_DELETE(m_current_iterator);
	m_current_iterator = new_it;
	m_collection_view_pane->ConstructView(m_current_iterator);

	TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::CollectionSortType, sort_type));
	TRAP(err, g_prefsManager->CommitL());
}

void OpCollectionView::SetMatchText(OpString& match_text)
{
	if (!m_current_iterator)
		return;

	if (match_text.IsEmpty())
	{
		m_match_text.Empty();
		OpStatus::Ignore(m_collection_view_pane->ConstructView(m_current_iterator));
		return;
	}

	if (m_match_text == match_text)
		return;

	RETURN_VOID_IF_ERROR(m_match_text.TakeOver(match_text));

	OpINT32Vector vector;
	if (m_current_iterator->IsRecent())
	{
		m_current_iterator->Reset();
		for (CollectionModelItem* citem = m_current_iterator->Next(); citem; citem = m_current_iterator->Next())
		{
			HotlistModelItem* item = static_cast<HotlistModelItem*>(citem);
			if (Matches(item, m_match_text))
			{
				item->SetMatched(true);
				RETURN_VOID_IF_ERROR(vector.Add(item->GetID()));
			}
			else
				item->SetMatched(false);
		}
	}
	else
	{
		bool all_bookmarks = true;
		HotlistModel* model = g_desktop_bookmark_manager->GetBookmarkModel();
		INT32 count = model->GetCount(); // this includes folders and all
		if (m_current_iterator->GetFolder())
		{
			count = static_cast<HotlistModelItem*>(m_current_iterator->GetFolder())->GetSubtreeSize();
			all_bookmarks = false;
		}

		INT32 index = 0;
		if (m_current_iterator->GetFolder())
		{
			HotlistModelItem* folder = static_cast<HotlistModelItem*>(m_current_iterator->GetFolder());
			if (folder && folder->GetChildIndex() != -1)
				index = folder->GetChildIndex();
		}

		// Start at first item or if inside a folder, first child
		for (INT32 i = index; (i-index) < count; i++)
		{
			HotlistModelItem* item = model->GetItemByIndex(i);
			if (all_bookmarks && item->GetIsTrashFolder())
			{
				// skip all items in trash
				i += item->GetSubtreeSize();
				continue;
			}

			if (item->IsFolder() || item->IsSeparator())
				continue;

			if (Matches(item, m_match_text))
			{
				item->SetMatched(true);
				RETURN_VOID_IF_ERROR(vector.Add(item->GetID()));
			}
			else
				item->SetMatched(false);
		}
	}
	m_collection_view_pane->ConstructViewInSearchMode(vector);
}

void OpCollectionView::OnBookmarkModelLoaded()
{
	OpenFolder(m_navigation_pane->GetBookmarkRootNavigationFolder());
	SetIgnoresMouse(FALSE);
}

BOOL OpCollectionView::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			 OpInputAction* child_action = action->GetChildAction();
			 switch (child_action->GetAction())
			 {
				case OpInputAction::ACTION_COPY:
				case OpInputAction::ACTION_COPY_TO_NOTE:
				case OpInputAction::ACTION_CUT:
				case OpInputAction::ACTION_PASTE:
				case OpInputAction::ACTION_SELECT_ALL:
				case OpInputAction::ACTION_FIND_NEXT:
				case OpInputAction::ACTION_PRINT_DOCUMENT:
				case OpInputAction::ACTION_PRINT_PREVIEW:
				case OpInputAction::ACTION_SET_ENCODING:
				case OpInputAction::ACTION_MANAGE_MODES:
					 child_action->SetEnabled(FALSE);
					 return TRUE;
			 }
		}
		break;

		case OpInputAction::ACTION_SHOW_CONTEXT_MENU:
			 return TRUE;
	}

	if (m_navigation_pane->OnInputAction(action))
		return TRUE;

	return m_collection_view_pane->OnInputAction(action);
}

inline bool OpCollectionView::Matches(HotlistModelItem* item, const OpStringC& match_text)
{
	return item->GetName().FindI(match_text) != KNotFound ||
		item->GetUrl().FindI(match_text) != KNotFound ||
		item->GetDescription().FindI(match_text) != KNotFound ||
		item->GetShortName().FindI(match_text) != KNotFound;
}
