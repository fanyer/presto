/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
*
* Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
*
* This file is part of the Opera web browser.  It may not be distributed
* under any circumstances.
*
* @file
* @author Owner:    Karianne Ekern (karie)
* @author Co-owner: Espen Sand (espen)
*
*/

#ifndef HOTLIST_PANEL_H
#define HOTLIST_PANEL_H

#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/quick_toolkit/widgets/OpToolbar.h"
#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "adjunct/quick/widgets/OpHotlistView.h"
#include "adjunct/quick/hotlist/Hotlist.h"


/***********************************************************************************
**
**	HotlistPanel
**
**
***********************************************************************************/

class HotlistPanel: 
	public OpWidget, 
	public OpDelayedTriggerListener
{
public:

	HotlistPanel();

	virtual void			SetToolbarName(const char* normal_name, const char* full_name);
	virtual void			SetFullMode(BOOL full_mode, BOOL force = FALSE);

	virtual Hotlist*		GetHotlist();
	virtual BOOL			IsFullMode() {return m_full_mode;}

	void 					SetSelectorButton(OpButton* selector_button) { m_selector_button = selector_button; }
	virtual	void			PanelChanged() {m_delayed_trigger.InvokeTrigger();}

	virtual void  			UpdateSettings() {}

	virtual void			OnLayout();

	// == OpDelayedTriggerListener ======================

	virtual void			OnTrigger(OpDelayedTrigger*) {if (listener) listener->OnChange(this, FALSE);}

	// panels must implement these

	virtual OP_STATUS		Init() = 0;

	virtual void			GetPanelText(OpString& text, Hotlist::PanelTextType text_type) = 0;

	virtual void			GetPanelName(OpString& name) {};
	virtual const char*		GetPanelImage() = 0;
	virtual Image			GetPanelIcon() { return Image(); }
	virtual BOOL			GetPanelAttention() {return FALSE;}

	virtual void			OnLayoutToolbar(OpToolbar* toolbar, OpRect& rect);
	virtual void			OnLayoutPanel(OpRect& rect) = 0;
	virtual void			OnFullModeChanged(BOOL full_mode) = 0;

	virtual void			SetSplitView() {}

	/**
	* Set the search text in the quick find field of the associated treeview
	* @param search text to be set
	*/
	virtual void 			SetSearchText(const OpStringC& search_text) {}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	virtual Accessibility::ElementKind AccessibilityGetRole() const { return Accessibility::kElementKindPanel; }
#endif

protected:
	OpButton*				m_selector_button;

private:

	OpToolbar*				m_normal_toolbar;
	OpToolbar*				m_full_toolbar;
	BOOL					m_full_mode;
	OpDelayedTrigger		m_delayed_trigger;
};

#endif //HOTLIST_PANEL_H
