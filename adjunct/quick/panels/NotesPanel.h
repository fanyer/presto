/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @file
 * @author Owner:    Karianne Ekern (karie)
 * @author Co-owner: Espen Sand (espen)
 *
 */

#ifndef NOTESPANEL_H
#define NOTESPANEL_H

#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "adjunct/desktop_util/treemodel/optreemodelitem.h"
#include "adjunct/quick/panels/HotlistPanel.h"

#include "modules/util/adt/opvector.h"

class OpToolbar;
class OpLabel;
class OpHotlistView;
class OpMultilineEdit;
class OpSplitter;

/***********************************************************************************
 **
 **	NotesPanel
 **
 ***********************************************************************************/

class NotesPanel : public HotlistPanel
{
	public:
			
								NotesPanel();

		OpHotlistView*			GetHotlistView() {return m_hotlist_view;}

		virtual OP_STATUS		Init();
		virtual void			GetPanelText(OpString& text, Hotlist::PanelTextType text_type);
		virtual const char*		GetPanelImage() {return "Panel Notes";}

		virtual void			OnLayoutPanel(OpRect& rect);
		virtual void			OnFullModeChanged(BOOL full_mode);
		virtual void			OnFocus(BOOL focus,FOCUS_REASON reason);
		virtual void			OnChange(OpWidget *widget, BOOL changed_by_mouse);
		virtual BOOL			OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked);
		virtual	void			OnDeleted();

	    INT32 		GetSelectedFolderID();
		HotlistModelItem*		GetSelectedItem() {return (HotlistModelItem*) m_hotlist_view->GetSelectedItem();}
	    void SetSelectedItemByID(INT32 id,
							 BOOL changed_by_mouse = FALSE,
							 BOOL send_onchange = TRUE,
							 BOOL allow_parent_fallback = FALSE)
	    {
			m_hotlist_view->SetSelectedItemByID(id, changed_by_mouse, send_onchange, allow_parent_fallback);
		}


		// helper function for other sources to create a note without knowing where to put them

		static void				NewNote(const uni_char* text, const uni_char* url = NULL);

		// Implementing the OpTreeModelItem interface

		virtual Type			GetType() {return PANEL_TYPE_NOTES;}

		// OpWidgetListener

		// == OpInputContext ======================

		virtual const char*		GetInputContextName() {return "Notes Panel";}
		virtual BOOL			OnInputAction(OpInputAction* action);

	private:

		OpSplitter*				m_splitter;
		OpMultilineEdit*		m_multiline_edit;
		OpHotlistView*			m_hotlist_view;
};

#endif // NOTESPANEL_H
