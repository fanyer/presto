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

#ifndef PANEL_DESKTOP_WINDOW_H
#define PANEL_DESKTOP_WINDOW_H

#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/quick_toolkit/widgets/OpToolbar.h"
#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "adjunct/quick/widgets/OpHotlistView.h"
#include "adjunct/quick/hotlist/Hotlist.h"
#include "adjunct/quick/panels/HotlistPanel.h"

class HotlistSelector;
class OpButton;


/***********************************************************************************
**
**	PanelDesktopWindow
**
**
***********************************************************************************/

class PanelDesktopWindow : public DesktopWindow
{
public:

	PanelDesktopWindow(OpWorkspace* parent_workspace);

	virtual void			OnLayout();
	virtual void			OnChange(OpWidget *widget, BOOL changed_by_mouse);

	void					SetPanelByType(Type panel_type);
	void					SetPanelByName(const uni_char* panel_name) {SetPanelByType(Hotlist::GetPanelTypeByName(panel_name));}

	Type					GetPanelType() {return m_panel ? m_panel->GetType() : UNKNOWN_TYPE;}
	const char*				GetPanelName() {return Hotlist::GetPanelNameByType(GetPanelType());}

	virtual const char*		GetWindowName();

	virtual Type			GetType() {return WINDOW_TYPE_PANEL;}

	virtual void			OnActivate(BOOL activate, BOOL first_time);

		virtual void			OnSessionReadL(const OpSessionWindow* session_window);
		virtual void			OnSessionWriteL(OpSession* session, OpSessionWindow* session_window, BOOL shutdown);
		virtual OP_STATUS		AddToSession(OpSession* session, INT32 parent_id, BOOL shutdown, BOOL extra_info);

	BOOL				OnInputAction(OpInputAction* action);

	HotlistPanel*		GetPanel() { return m_panel; }

	/**
	* Set the search text in the quick find field of the associated treeview
	* @param search text to be set
	*/
	void SetSearch(const OpStringC& search);

private:

	void					UpdateTitle();

	HotlistPanel*			m_panel;
	OpString8				m_window_name;
};

#endif //HOTLIST_H
