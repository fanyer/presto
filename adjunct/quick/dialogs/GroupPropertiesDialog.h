/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef GROUPPROPERTIES_H
#define GROUPPROPERTIES_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/desktop_util/treemodel/simpletreemodel.h"

/***********************************************************************************
**
**	GroupPropertiesDialog
**
***********************************************************************************/

class GroupPropertiesDialog : public Dialog
{
	public:
			
		void					Init(DesktopWindow* parent_window, OpTreeModelItem* item, OpTreeModelItem* parent = NULL);

								GroupPropertiesDialog();
		virtual					~GroupPropertiesDialog() {};

		DialogType				GetDialogType(){return TYPE_PROPERTIES;}
		Type					GetType()				{return DIALOG_TYPE_GROUP_PROPERTIES;}
		const char*				GetWindowName()			{return "Group Properties Dialog";}
		BOOL					GetModality()			{return FALSE;}
		const char*				GetHelpAnchor()			{return "panels.html#contacts";}

		virtual void			OnInit();
		virtual UINT32			OnOk();
		virtual void			OnCancel();

	private:

		OpTreeModelItem*		m_group;
		INT32					m_group_id;
		INT32					m_parent_id;

		SimpleTreeModel			m_group_model;
};

#endif
