/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_SEARCH_EDIT_H
#define OP_SEARCH_EDIT_H

#include "modules/widgets/OpEdit.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "adjunct/quick/managers/FavIconManager.h"

/***********************************************************************************
 **
 **	OpSearchEdit
 **
 **
 **
 **
 ***********************************************************************************/
//
// (Note: When Toolbar reads Search it sets the (OpTypedObject) ID of the SearchEdit to
// the same as the Search it 'represents')

class OpSearchEdit : public OpEdit, 
					 public DocumentDesktopWindowSpy, 
					 public FavIconManager::FavIconListener
{
	public:
								OpSearchEdit(INT32 index_or_negated_search_type = 0);
								OpSearchEdit(const OpString& guid);

	static OP_STATUS			Construct(OpSearchEdit** obj);

	INT32                   GetSearchEditIdentifier() { return m_index_or_negated_search_type; }
	const OpStringC&		GetSearchEditUniqueGUID() const { return g_searchEngineManager->GetSearchEngine(m_index_in_search_model)->GetUniqueGUID(); }
	INT32					GetSearchIndex() {return m_index_in_search_model;} // Doh, get rid of this!

		void					SetSearchInSpeedDial(BOOL speed_dial) { m_search_in_speeddial = speed_dial; }

		void					SetSearchType(INT32 search_type) { UpdateSearch(search_type); }

		enum SEARCH_STATUS {
			SEARCH_STATUS_NORMAL,
			SEARCH_STATUS_NOMATCH
		};

		/** Set current search status. Will show extra indication by changing color */
		void					SetSearchStatus(SEARCH_STATUS status);

		virtual void			OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect);

		virtual void			OnDeleted();

		// OpWidgetListener

		virtual void			OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y);
		virtual void			OnDragMove(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y) { static_cast<OpWidgetListener *>(GetParent())->OnDragMove(this, drag_object, pos, x, y);}
		virtual void			OnDragDrop(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y) { static_cast<OpWidgetListener *>(GetParent())->OnDragDrop(this, drag_object, pos, x, y);}
		virtual void			OnDragLeave(OpWidget* widget, OpDragObject* drag_object) { static_cast<OpWidgetListener *>(GetParent())->OnDragLeave(this, drag_object);}
		virtual BOOL            OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint &menu_point, const OpRect *avoid_rect, BOOL keyboard_invoked);

		virtual void			OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks) {((OpWidgetListener*)GetParent())->OnMouseEvent(this, pos, x, y, button, down, nclicks);}
		virtual void			OnSettingsChanged(DesktopSettings* settings);
	
		virtual void			OnChange(OpWidget *widget, BOOL changed_by_mouse);

		// OpInputStateListener

		virtual void			OnUpdateActionState() {UpdateSearchIcon(); OpEdit::OnUpdateActionState();}

		// == OpInputContext ======================

		virtual BOOL			OnInputAction(OpInputAction* action);

		virtual Type			GetType() {return WIDGET_TYPE_SEARCH_EDIT;}

		// FavIconManager::FavIconListener
		virtual void 			OnFavIconAdded(const uni_char* document_url, const uni_char* image_path);
		virtual void 			OnFavIconsRemoved();

		void					Search(OpInputAction* action);

		void					SetForceSearchInpage(BOOL force_search_in_page);

	private:
		void					Initialize(INT32 index_or_negated_search_type);
		INT32					TranslateType(INT32 search_type);
		void					UpdateSearchIcon();
		void					UpdateSearch(INT32 search_type);

	// This weird animal is currently used when constructing the SearchEdit,
	// it is sent along when dragging the SearchEdit to identify it,
	// and it is used for ACTION_SEARCH
	    INT32					m_index_or_negated_search_type;

	// This is the index in the vector of searches inside the SearchEngineManager
		INT32					m_index_in_search_model;

		BOOL					m_search_in_page;
		BOOL					m_search_in_speeddial;
		SEARCH_STATUS			m_status;
};

#endif // OP_SEARCH_EDIT_H
