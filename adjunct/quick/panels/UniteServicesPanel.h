/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @file
 * @author Owner:    Karianne Ekern (karie)
 *
 */

#ifndef UNITESERVICESPANEL_H
#define UNITESERVICESPANEL_H

#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "modules/util/adt/opvector.h"

#include "adjunct/quick/panels/HotlistPanel.h"


#ifdef WEBSERVER_SUPPORT

class OpToolbar;
class OpLabel;
class OpHotlistView;
class OpMultilineEdit;

/***********************************************************************************
 **
 **	UniteServicesPanel
 **
 ***********************************************************************************/

class UniteServicesPanel : public HotlistPanel, public OpTreeModel::Listener
{
	public:
			
								UniteServicesPanel();
		virtual					~UniteServicesPanel();

		OpHotlistView*			GetHotlistView() {return m_hotlist_view;}

		virtual OP_STATUS		Init();
		virtual void			GetPanelText(OpString& text, Hotlist::PanelTextType text_type);
		virtual const char*		GetPanelImage() {return "Panel Unite";}

		virtual void			OnLayoutPanel(OpRect& rect);
		virtual void			OnFullModeChanged(BOOL full_mode);
		virtual void			OnFocus(BOOL focus,FOCUS_REASON reason);
		virtual void			OnRemoved();

		virtual BOOL			GetPanelAttention() { return m_needs_attention > 0; }

		// Implementing the OpTreeModelItem interface

		virtual Type			GetType() {return PANEL_TYPE_UNITE_SERVICES;}

		// OpWidgetListener

		// == OpInputContext ======================

		virtual const char*		GetInputContextName() {return "Unite Services Panel";}
		virtual BOOL			OnInputAction(OpInputAction* action);

		// == OpTreeModel::Listener ======================
		
		void					OnItemAdded(OpTreeModel* tree_model, INT32 pos) {}
		void					OnItemChanged(OpTreeModel* tree_model, INT32 pos, BOOL sort);
		void					OnItemRemoving(OpTreeModel* tree_model, INT32 pos) {}
		void					OnSubtreeRemoving(OpTreeModel* tree_model, INT32 parent, INT32 pos, INT32 subtree_size) {}
		void					OnSubtreeRemoved(OpTreeModel* tree_model, INT32 parent, INT32 pos, INT32 subtree_size) {}
		void					OnTreeChanging(OpTreeModel* tree_model) {}
		void					OnTreeChanged(OpTreeModel* tree_model) {}
		void					OnTreeDeleted(OpTreeModel* tree_model) {}
	
	private:

		OpHotlistView*			m_hotlist_view;
		int						m_needs_attention;

};

#endif // WEBSERVER_SUPPORT

#endif // GADGETSPANEL_H
