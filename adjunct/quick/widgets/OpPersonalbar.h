/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_PERSONALBAR_H
#define OP_PERSONALBAR_H

#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpButton.h"
#include "adjunct/quick_toolkit/widgets/OpToolbar.h"
#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "adjunct/desktop_util/treemodel/optreemodelitem.h"
#include "modules/util/adt/opvector.h"
#include "adjunct/quick/managers/DesktopBookmarkManager.h"

/***********************************************************************************
**
**	OpPersonalbar
**
***********************************************************************************/

class OpPersonalbar : public OpToolbar, public OpTreeModel::Listener, public DesktopBookmarkListener
{
	protected:
		OpPersonalbar(PrefsCollectionUI::integerpref prefs_setting);

	public:
		static OP_STATUS	Construct(OpPersonalbar** obj);

		virtual INT32		GetDragSourcePos(DesktopDragObject* drag_object);
		virtual void		Write();

		// == Hooks ======================

		virtual void		OnDeleted();
		virtual void		OnFocus(BOOL focus,FOCUS_REASON reason);
		virtual void		OnLayout(BOOL compute_size_only, INT32 available_width, INT32 available_height, INT32& used_width, INT32& used_height);
		virtual void		OnReadContent(PrefsSection *section) {}
		virtual void		OnWriteContent(PrefsFile* prefs_file, const char* name) {}

		// Implementing the OpWidgetListener interface

		virtual void		OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y);
		virtual void		OnDragMove(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
		virtual void		OnDragDrop(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y);
		virtual void		OnDragLeave(OpWidget* widget, OpDragObject* drag_object);
		virtual BOOL        OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked);

		// OpWidget overrides
		virtual void		OnShow(BOOL show);

		// == OpInputContext ======================

		virtual BOOL			OnInputAction(OpInputAction* action);
		virtual const char*		GetInputContextName() {return "Personalbar"; }

		// == OpTreeModel::Listener ======================

		void				OnItemAdded(OpTreeModel* tree_model, INT32 pos);
		void				OnItemRemoving(OpTreeModel* tree_model, INT32 pos);
		void				OnItemChanged(OpTreeModel* tree_model, INT32 pos, BOOL sort);
		void				OnSubtreeRemoving(OpTreeModel* tree_model, INT32 parent, INT32 pos, INT32 subtree_size) {}
		void				OnSubtreeRemoved(OpTreeModel* tree_model, INT32 parent, INT32 pos, INT32 subtree_size) {}
		void				OnTreeChanging(OpTreeModel* tree_model) {}
		void				OnTreeChanged(OpTreeModel* tree_model);
		void				OnTreeDeleted(OpTreeModel* tree_model) {}

		// == DesktopBookmarkListener =====================
		void				OnBookmarkModelLoaded();

		// == OpTreeModelItem ======================

		virtual	Type		GetType() {return WIDGET_TYPE_PERSONALBAR;}

		virtual void		EnableTransparentSkin(BOOL enable);

		// is this personal bar inline inside a tab or in the top level window
		virtual BOOL		IsInline() { return m_is_inline; }

		// reset the tab stop again
		virtual void		UpdateWidget(OpWidget* widget) { OpToolbar::UpdateWidget(widget); widget->SetTabStop(TRUE); }

		virtual BOOL		DisableDropPositionIndicator() {return m_disable_indicator;}

	    INT32               GetPosition(OpWidget* wdg);
private:

		BOOL				AddItem(OpTreeModel* model, INT32 model_pos, BOOL init = FALSE);
		BOOL				RemoveItem(OpTreeModel* model, INT32 model_pos);
		BOOL				ChangeItem(OpTreeModel* model, INT32 model_pos);
		INT32				FindItem(INT32 id);
		INT32				FindItem(INT32 id, OpTreeModel* model_that_id_belongs_to);
		// @param pos position of the button, not the model item
		HotlistModelItem*   GetModelItemFromPos(int pos);

		void				DragMoveDrop(OpWidget* widget, OpDragObject* drag_object, INT32 pos, INT32 x, INT32 y, BOOL drop);

protected:

		BOOL				m_is_inline;	// TRUE if inline in a tab, FALSE if on the top level window
		BOOL				m_listeners_set;	// TRUE if listeners are set
		BOOL				m_disable_indicator;
};

// regular personal bar, except tailored for use inline in tabs and not in the top level browser window
class OpPersonalbarInline : public OpPersonalbar
{
public:
	OpPersonalbarInline(PrefsCollectionUI::integerpref prefs_setting);
	
	static OP_STATUS	Construct(OpPersonalbarInline** obj);
	
	virtual	Type		GetType() {return WIDGET_TYPE_PERSONALBAR_INLINE;}
	virtual const char*	GetInputContextName() {return "Personalbar inline"; }

	virtual void		EnableTransparentSkin(BOOL enable);
};

#endif // OP_PERSONALBAR_H
