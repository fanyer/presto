/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
*
* Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
*
* This file is part of the Opera web browser.	It may not be distributed
* under any circumstances.
*
*/

#ifndef OP_COLLECTION_VIEW_H
#define OP_COLLECTION_VIEW_H

#include "adjunct/quick/managers/DesktopBookmarkManager.h"
#include "adjunct/quick/widgets/DocumentView.h"
#include "adjunct/quick_toolkit/widgets/OpSplitter.h"

class CollectionViewPane;
class NavigationFolderItem;
class NavigationItem;
class NavigationModel;
class CollectionNavigationPane;

/**
 * @brief OpCollectionView is a container.
 *
 * OpCollectionView is a container which hosts two panes:
 * a. NavigationPane
 * b. ViewPane
 *
 * These panes are separated by splitter.
 */
class OpCollectionView
	: public OpSplitter
	, public DocumentView
	, public DesktopBookmarkListener
{
	friend class CollectionNavigationPane;

public:
	OpCollectionView();
	~OpCollectionView();

	/** Initializes OpCollectionView.
	  * @returns OpStatus::OK if successfully initialized, otherwise error
	  */
	OP_STATUS Initialize();

	/** Opens selected folder
	  * After an item(folder) is selected in NavigationPane, its content
	  * is shown in ViewPane
	  * @param item represents one of the folder in NavigationPane
	  */
	void OpenSelectedFolder(NavigationItem* item);

	/** Sorts items in viewpane
	  * @param type identifies two types of sorting
	  * a. SORT_BY_CREATED and b. SORT_BY_NAME
	  */
	void SortItems(CollectionModelIterator::SortType type);

	/** @return reference to CollectionViewPane
	*/
	CollectionViewPane& GetCollectionViewPane() const { return *m_collection_view_pane; }

	/** @return reference to current iterator
	*/
	CollectionModelIterator& GetCurrentIterator() const { return *m_current_iterator; }

	// OpInputContext
	virtual const char* GetInputContextName() { return "Bookmarks Widget"; }
	virtual void OnKeyboardInputGained(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason) {
		g_input_manager->UpdateAllInputStates(); }

	// OpWidget;
	virtual Type					GetType() {return WIDGET_TYPE_BOOKMARKS_VIEW;}
	virtual void					OnRemoved() { g_pcui->WriteIntegerL(PrefsCollectionUI::CollectionSplitter, GetDivision()); }

	// OpSplitter(OpSplitter->OpGroup->OpWidget)
	virtual BOOL					OnInputAction(OpInputAction* action);

	// DocumentView
	virtual OP_STATUS				Create(OpWidget* container);
	virtual void					Show(bool visible) { SetVisibility(visible); }
	virtual void					SetFocus(FOCUS_REASON reason) { OpSplitter::SetFocus(reason); }
	virtual bool					IsVisible() { return OpSplitter::IsVisible() ? true : false; }
	virtual OpWidget*				GetOpWidget() { return this; }
	virtual void					SetRect(OpRect rect) { OpSplitter::SetRect(rect); }
	virtual void					Layout() { OpSplitter::OnLayout(); }
	virtual DocumentViewTypes		GetDocumentType() { return DOCUMENT_TYPE_COLLECTION; }
	virtual OP_STATUS				GetTitle(OpString& title) { return g_languageManager->GetString(Str::S_NAVIGATION_PANE_TITLE, title); }
	virtual void					GetThumbnailImage(Image& image);
	virtual bool					HasFixedThumbnailImage() { return true; }
	virtual OP_STATUS				GetTooltipText(OpInfoText& text);
	virtual const OpWidgetImage*	GetFavIcon(BOOL force) { return &m_doc_view_fav_icon; }

	// DocumentView->OpZoomSliderSettings
	virtual double					GetWindowScale();
	virtual void					QueryZoomSettings(double &min, double &max, double &step, const OpSlider::TICK_VALUE* &tick_values, UINT8 &num_tick_values);

	// DesktopBookmarkListener
	virtual void					OnBookmarkModelLoaded();

	// Hooks for subclasses
	virtual BOOL					IsSingleClick() {return g_pcui->GetIntegerPref(PrefsCollectionUI::HotlistSingleClick);}
	virtual	INT32					GetRootID()	{return HotlistModel::BookmarkRoot;}

private:
	static bool Matches(HotlistModelItem* item, const OpStringC& match_text);

	// Called when a external object dragged to treeviews
	void							OpenFolder(NavigationItem* folder);
	void							SetMatchText(OpString& match_text);

	CollectionViewPane*				m_collection_view_pane;
	CollectionNavigationPane*		m_navigation_pane;
	CollectionModelIterator*		m_current_iterator;
	OpString						m_match_text;
	OpWidgetImage					m_doc_view_fav_icon;
	NavigationItem*					m_current_selected_item;
};

#endif // OP_COLLECTION_VIEW_H
