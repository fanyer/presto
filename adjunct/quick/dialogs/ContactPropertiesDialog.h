/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef CONTACTPROPERTIES_H
#define CONTACTPROPERTIES_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/desktop_util/treemodel/optreemodel.h"

class HotlistModel;
class HotlistModelItem;

class ContactPropertiesDialog : public Dialog, public OpTreeModel::Listener
{
public:
			
	OP_STATUS				Init(DesktopWindow* parent_window, HotlistModel* model, HotlistModelItem* item, HotlistModelItem* parent = NULL);
	//DesktopWindow* parent_window, OpTreeModelItem* item, OpTreeModelItem* parent = NULL);
	virtual					~ContactPropertiesDialog() {};

	DialogType				GetDialogType(){return TYPE_PROPERTIES;}
	Type					GetType()				{return DIALOG_TYPE_CONTACT_PROPERTIES;}
	const char*				GetWindowName()			{return "Contact Properties Dialog";}
	BOOL					GetModality()			{return FALSE;}
	const char*				GetHelpAnchor()			{return "panels.html#contacts";}

	virtual void			OnInit();
	virtual UINT32			OnOk();
	virtual void			OnCancel();	
	virtual void 			OnClose(BOOL user_initiated);
	HotlistModelItem* 		GetContact();
	
	virtual	void			OnItemAdded(OpTreeModel* tree_model, INT32 item) {}
	virtual	void			OnItemChanged(OpTreeModel* tree_model, INT32 item, BOOL sort) {}
	virtual	void			OnItemRemoving(OpTreeModel* tree_model, INT32 item) {}
	virtual	void			OnSubtreeRemoving(OpTreeModel* tree_model, INT32 parent, INT32 subtree_root, INT32 subtree_size) {}
	virtual	void			OnSubtreeRemoved(OpTreeModel* tree_model, INT32 parent, INT32 subtree_root, INT32 subtree_size) {}
	virtual void			OnTreeChanging(OpTreeModel* tree_model ) {}
	virtual void			OnTreeChanged(OpTreeModel* tree_model);
	virtual void			OnTreeDeleted(OpTreeModel* tree_model);

private:
	HotlistModel* 			m_model;
	INT32					m_contact_id;
	INT32					m_parent_id;
	HotlistModelItem* 		m_temporary_item;
	INT32                   m_personalbar_position; // Not choosable in properties dialog, must be stored to keep value after setting new properties
};

#endif
