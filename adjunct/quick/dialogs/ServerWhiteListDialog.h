/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @file
 * @author Owner:    Karianne Ekern (karie)
 * @author Co-owner: Adam Minchinton (adamm)
 *
 */

#ifndef SERVER_WHITELISTE_DIALOG_H
#define SERVER_WHITELISTE_DIALOG_H

#include "SimpleDialog.h"
#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/quick/models/ServerWhiteList.h"

class ServerWhiteListDialog : public Dialog
{
public:

	enum ListType
	{
		WhiteList,
		SecureInternalHosts 
	};

	ServerWhiteListDialog();
	virtual ~ServerWhiteListDialog();
	
	Type				GetType()			{return DIALOG_TYPE_SERVER_WHITELIST;}
	DialogType			GetDialogType()			{return TYPE_PROPERTIES;}
	const char*			GetWindowName()			{return "Server Whitelist Dialog";}
	const char*			GetHelpAnchor()			{return "contentblock.html";}
	BOOL				GetModality()			{return TRUE;}
	BOOL				GetIsBlocking()			{return TRUE;}
	void				OnInit(); 
	void				OnCancel();
	UINT32				OnOk();
	void				OnClick(OpWidget *widget, UINT32 id);
	void				Init(DesktopWindow *parent);
	INT32				GetButtonCount()		{return 1;}
	void				GetButtonInfo(INT32 index, OpInputAction*& action, OpString& text, BOOL& is_enabled, BOOL& is_default, OpString8& name);
	void				OnInitPage(INT32 page_number, BOOL first_time);

	void				OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);

	// OpWidgetListener
	void OnItemEdited(OpWidget *widget, INT32 pos, INT32 column, OpString& text);
	void OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y) {}

	// return false in case of failure,the item will be deleted
	BOOL				OnItemEdit(const OpString& old_item,const OpString& new_item);
	BOOL				OnItemDelete(const uni_char* item);
	BOOL				IsItemEditable(const OpStringC& item);
	const uni_char*		GetDefaultNewItem();

	SimpleTreeModel*	GetOverrideHostsModel(); // postpone the initializtion of m_override_hosts_model
private:
	SimpleTreeModel		*m_whitelist_model;			
	SimpleTreeModel		*m_override_hosts_model;		
	SimpleTreeModel		*m_current_model;
};

#endif // SERVER_WHITELIST_DIALOG_H
