/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @file
 * @author Owner:    Karianne Ekern (karie)
 * @author Co-owner: Espen Sand (espen)
 *
 */

#ifndef HOTLIST_DESKTOP_WINDOW_H
#define HOTLIST_DESKTOP_WINDOW_H

#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/quick_toolkit/widgets/OpToolbar.h"
#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "adjunct/quick/widgets/OpHotlistView.h"
#include "adjunct/quick/hotlist/Hotlist.h"
#include "adjunct/quick/panels/HotlistPanel.h"

//class HotlistPanel;
class HotlistSelector;
class OpButton;



/***********************************************************************************
 **
 **	HotlistDesktopWindow
 **
 **
 ***********************************************************************************/

class HotlistDesktopWindow : public DesktopWindow
{
	public:
			
								HotlistDesktopWindow(OpWorkspace* parent_workspace, Hotlist* hotlist = NULL);
		virtual					~HotlistDesktopWindow() {};

		virtual void			OnRelayout(OpWidget* widget);
		virtual void			OnChange(OpWidget *widget, BOOL changed_by_mouse);
		virtual void			OnLayout();
		virtual OP_STATUS		AddToSession(OpSession* session, INT32 parent_id, BOOL shutdown, BOOL extra_info = FALSE) {return OpStatus::OK;}

		virtual const char*		GetWindowName() {return "Hotlist Window";}

		virtual Type			GetType() {return WINDOW_TYPE_HOTLIST;}

	private:

		void					UpdateTitle();

		Hotlist*				m_hotlist;
};

#endif //HOTLIST_DESKTOP_WINDOW_H
