/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 *
 */

#ifndef PROXY_EXCEPTION_DIALOG_H
#define PROXY_EXCEPTION_DIALOG_H

#include "SimpleDialog.h"
#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/quick/models/ServerWhiteList.h"

class ProxyExceptionDialog : public Dialog
{
public:
	ProxyExceptionDialog();
	virtual ~ProxyExceptionDialog();
	
	DialogType			GetDialogType()		{return TYPE_OK_CANCEL;}
	const char*			GetWindowName()		{return "Proxy Exception Dialog";}
	BOOL				GetModality()		{return TRUE;}
	void				OnInit();
	UINT32				OnOk();

	void				OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);
    BOOL    			OnInputAction(OpInputAction* action);

	// OpWidgetListener
	void OnItemEdited(OpWidget *widget, INT32 pos, INT32 column, OpString& text);
	void OnChange(OpWidget *widget, BOOL changed_by_mouse = FALSE);

	// return false in case of failure,the item will be deleted
	BOOL				OnItemEdit(const OpString& old_item, OpString& new_item);
	BOOL				OnItemDelete(const uni_char* item);


	SimpleTreeModel*	GetWhitelistModel(); // postpone the initializtion of m_whitelist_model
	SimpleTreeModel*	GetBlacklistModel(); // postpone the initializtion of m_blacklist_model
private:
	SimpleTreeModel		*m_whitelist_model;			
	SimpleTreeModel		*m_blacklist_model;		
	SimpleTreeModel		*m_current_model;
	OpTreeView*			m_treeview;
};

#endif // SERVER_WHITELIST_DIALOG_H
