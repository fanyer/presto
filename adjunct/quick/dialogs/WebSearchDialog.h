// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
//
// Copyright (C) 1995-2003 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Espen Sand
//
#ifndef __WEB_SEARCH_DIALOG_H__
#define __WEB_SEARCH_DIALOG_H__

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/quick/widgets/OpSearchDropDown.h"
#include "modules/widgets/OpEdit.h"

/*********************************************************************
 *
 * WebSearchDialog
 *
 * Small dialog with an OpSearchDropDown 
 * (Action Show Web Search / ACTION_SHOW_WEB_SEARCH)
 * 
 ********************************************************************/

class WebSearchDialog : public Dialog, 
						public OpSearchDropDownListener
{
public:
	WebSearchDialog() { m_search_done = FALSE; }
	~WebSearchDialog();

	static void Create(DesktopWindow* parent_window );
 
	virtual void				OnInit();

	virtual BOOL				GetModality()			{return TRUE;}
	virtual Type				GetType()				{return DIALOG_TYPE_WEB_SEARCH;}
	virtual const char*			GetWindowName()			{return "Web Search Dialog";}

	INT32						GetButtonCount() { return 2; };
	
	void 						OnSearchChanged( const OpStringC& search_guid );

	void 						OnSearchDone();
	BOOL 						OnInputAction(OpInputAction* action);


private:
	static WebSearchDialog* m_dialog;
	OpEdit* m_edit;
	BOOL m_search_done;
};

#endif
