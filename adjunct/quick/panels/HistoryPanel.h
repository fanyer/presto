/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef HISTORY_PANEL_H
#define HISTORY_PANEL_H

#include "modules/util/adt/opvector.h"

#include "adjunct/quick/panels/HotlistPanel.h"

class OpHistoryView;

/***********************************************************************************
 **
 **	HistoryPanel
 **
 ***********************************************************************************/

class HistoryPanel : public HotlistPanel
{
 public:
			
                                HistoryPanel();
    virtual                     ~HistoryPanel() {};

    virtual OP_STATUS	Init();
    virtual void		GetPanelText(OpString& text, Hotlist::PanelTextType text_type);
    virtual const char*	GetPanelImage() {return "Panel History";}
    virtual void		OnLayoutPanel(OpRect& rect);
    virtual void		OnFullModeChanged(BOOL full_mode);
    virtual void		OnFocus(BOOL focus,FOCUS_REASON reason);

    virtual Type		GetType() {return PANEL_TYPE_HISTORY;}

	/**
	 * Set the search text in the quick find field of the associated treeview
	 * @param search text to be set
	 */
	virtual void 		SetSearchText(const OpStringC& search_text);

    // == OpInputContext ======================

    virtual BOOL		OnInputAction(OpInputAction* action);
    virtual const char*	GetInputContextName() {return "History Panel";}

 private:
    OpHistoryView* m_history_view;
};

#endif // HISTORY_PANEL_H
