/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @file
 * @author Owner:    Karianne Ekern (karie)
 * @author Co-owner: Espen Sand (espen)
 *
 */

#ifndef HOTLIST_H
#define HOTLIST_H

#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "adjunct/quick/managers/DesktopBookmarkManager.h"
#include "adjunct/quick/widgets/OpHotlistView.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/quick_toolkit/widgets/OpToolbar.h"

#include "modules/widgets/OpButton.h"

class HotlistPanel;
class HotlistSelector;
class HotlistConfigButton;

/***********************************************************************************
 **
 **	Hotlist
 **
 **
 ***********************************************************************************/

class Hotlist : 
	public OpBar,
	public OpTreeModel::Listener,
	public DesktopBookmarkListener
{
	public:

		enum PanelTextType
		{
			PANEL_TEXT_NAME,	// name of panel "Transfers", "Mail"
			PANEL_TEXT_LABEL,	// label for button (must be short) like "Mail (5)" or "00:32 (3)" for transfers
			PANEL_TEXT_TITLE	// text for window title or tooltip. can be longer, like "Transfers 00:32 (3)"
		};

								Hotlist(PrefsCollectionUI::integerpref prefs_setting = PrefsCollectionUI::DummyLastIntegerPref);

		static const char*		GetPanelNameByType(Type type);
		static Type				GetPanelTypeByName(const uni_char* name);
		static HotlistPanel*	CreatePanelByType(Type type);

		void					AddPanel(HotlistPanel* panel, INT32 pos = -1);
		void					AddPanelByType(Type type, INT32 pos = -1);
		HotlistPanel*			GetPanelByType(Type type, INT32* index = NULL);
		// @param focus Only make sense when show is TRUE. Open the panel if TRUE, otherwise just add the panel icon. 
		//				If the panel is already open it will remain open.
		BOOL					ShowPanelByType(OpTypedObject::Type type, BOOL show, BOOL focus = TRUE);
		BOOL					ShowPanelByName(const uni_char* name, BOOL show, BOOL focus = TRUE) {return ShowPanelByType(GetPanelTypeByName(name), show, focus);}
		
		//put the specified panel type on a the target position. It will move the panel if it already is there
		//returns the panel that has been placed on the position
		HotlistPanel*			PositionPanel(Type panel_type, INT32 pos);

		BOOL					IsPanelVisible(const uni_char* name) {return GetPanelByType(GetPanelTypeByName(name)) != NULL;}
		BOOL					IsCollapsableToSmall();

		virtual Collapse		TranslateCollapse(Collapse collapse);

		INT32					GetPanelCount();
		HotlistPanel*			GetPanel(INT32 index);
		void					GetPanelText(INT32 index, OpString& text, PanelTextType text_type);
		OpWidgetImage*			GetPanelImage(INT32 index);

		INT32					GetSelectedPanel();
		void					SetSelectedPanel(INT32 index);

		void					RemovePanel(INT32 index);
		void					MovePanel(INT32 src, INT32 dst);
		BOOL					ShowContextMenu(const OpPoint &point, BOOL center, BOOL use_keyboard_context);

		virtual	BOOL			GetLayout(OpWidgetLayout& layout);

		virtual void			OnReadPanels(PrefsSection *section);
		virtual void			OnWritePanels(PrefsFile* prefs_file, const char* name);

		virtual void			OnRelayout();
		virtual void			OnLayout(BOOL compute_size_only, INT32 available_width, INT32 available_height, INT32& used_width, INT32& used_height);
		virtual void			OnAdded() { if (GetResultingAlignment() != ALIGNMENT_OFF && !IsInitialized()) Init(); }
		virtual void			OnFocus(BOOL focus,FOCUS_REASON reason);
		virtual void			OnDeleted();
		virtual BOOL			SyncAlignment() { return FALSE; }

		virtual Type			GetType() {return WIDGET_TYPE_HOTLIST;}

		// if we're COLLAPSE_SMALL, there is only the hotlist button row visible, so we're not resizable
		virtual BOOL			GetIsResizable() { return GetCollapse() == COLLAPSE_NORMAL; }

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	virtual Accessibility::ElementKind AccessibilityGetRole() const {return Accessibility::kElementKindPanel;}
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

		// Implementing the OpInputConext interface

		virtual const char*	GetInputContextName() {return "Panels";}
		BOOL					OnInputAction(OpInputAction* action);

		// Implementing the OpWidgetListener interface
	
		virtual void			OnChange(OpWidget *widget, BOOL changed_by_mouse);
		virtual void			OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y);
		virtual void			OnDragMove(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
		virtual void			OnDragDrop(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y);

		// == OpTreeModel::Listener ======================

		void					OnItemAdded(OpTreeModel* tree_model, INT32 pos);
		void					OnItemChanged(OpTreeModel* tree_model, INT32 pos, BOOL sort);
		void					OnItemRemoving(OpTreeModel* tree_model, INT32 pos);
		void					OnSubtreeRemoving(OpTreeModel* tree_model, INT32 parent, INT32 pos, INT32 subtree_size) {}
		void					OnSubtreeRemoved(OpTreeModel* tree_model, INT32 parent, INT32 pos, INT32 subtree_size) {}
		void					OnTreeChanging(OpTreeModel* tree_model) {}
		void					OnTreeChanged(OpTreeModel* tree_model);
		void					OnTreeDeleted(OpTreeModel* tree_model) {}

		void					SaveActiveTab(UINT index);
		INT32					ReadActiveTab();

		// DesktopBookmarkListener
		virtual void			OnBookmarkModelLoaded();

		virtual OP_STATUS		Init();
		BOOL					IsInitialized() { return m_selector && OpStatus::IsSuccess(init_status); }

	private:

		BOOL					AddItem(OpTreeModel* model, INT32 model_pos);
		BOOL					RemoveItem(OpTreeModel* model, INT32 model_pos);
		BOOL					ChangeItem(OpTreeModel* model, INT32 model_pos);
		INT32					FindItem(INT32 id);
		INT32					FindItem(OpString guid);

		HotlistSelector*		m_selector;
		OpToolbar*				m_header_toolbar;
};


/***********************************************************************************
 **
 **	HotlistSelector
 **
 ***********************************************************************************/

class HotlistSelector : public OpToolbar
{
	public:
								HotlistSelector(Hotlist* hotlist);
		virtual					~HotlistSelector() {}

		void					MoveWidget(INT32 src, INT32 dst);
		BOOL					SetSelected(INT32 index, BOOL invoke_listeners = FALSE, BOOL changed_by_mouse = FALSE);

		INT32					GetItemCount();	//< Returns the number of items in the hotlist (excluding the function button visible in new skins)
		virtual void			GetPadding(INT32* left, INT32* top, INT32* right, INT32* bottom);

		virtual void			OnReadContent(PrefsSection *section) {m_hotlist->OnReadPanels(section);}
		virtual void			OnWriteContent(PrefsFile* prefs_file, const char* name) {m_hotlist->OnWritePanels(prefs_file, name);}
		virtual void			OnLayout();
		virtual void			OnRelayout();
		virtual BOOL			SetAlignment(Alignment alignment, BOOL write_to_prefs = FALSE);


		// == OpInputContext ======================

		virtual BOOL			OnInputAction(OpInputAction* action);
		virtual void			OnClick(OpWidget *widget, UINT32 id = 0);
		virtual BOOL            OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked);

		// Implementing the OpTreeModelItem interface

		virtual Type		GetType() {return WIDGET_TYPE_PANEL_SELECTOR;}

	private:

		Hotlist*				m_hotlist;
		OpToolbar*				m_floating_bar;
};


#endif //HOTLIST_H
