/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef CUSTOMIZE_HEADER_DIALOG_H
#define CUSTOMIZE_HEADER_DIALOG_H

#ifdef M2_SUPPORT

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/desktop_util/treemodel/simpletreemodel.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "modules/widgets/OpEdit.h"

class HeaderDisplay;

/***********************************************************************************
**
**	CustomizeHeaderDialog
**
***********************************************************************************/

class CustomizeHeaderDialog : public Dialog, public DialogListener
{
	public:
								CustomizeHeaderDialog();
		virtual					~CustomizeHeaderDialog();

		void					Init(DesktopWindow* parent_window);

		virtual const char*		GetWindowName()			{return "Customize Header Dialog";}

		virtual void			OnInit();
		virtual UINT32			OnOk();
		virtual void			OnCancel();

		virtual void			OnClick(OpWidget *widget, UINT32 id);

		virtual void			OnOk(Dialog* dialog, UINT32 result);

		virtual void			OnChange(OpWidget *widget, BOOL changed_by_mouse = FALSE);

	private:
		SimpleTreeModel			m_header_model;
		OpTreeView*				m_tree_view;
		OpButton*				m_up_button;
		OpButton*				m_down_button;
		OpButton*				m_add_button;
		OpButton*				m_delete_button;
		HeaderDisplay*			m_display;
		OpINT32Vector			m_moves;
};

/***********************************************************************************
**
**	AddCustomHeaderDialog
**
***********************************************************************************/

class AddCustomHeaderDialog : public Dialog
{
	public:
								AddCustomHeaderDialog();
		virtual					~AddCustomHeaderDialog();

		void					Init(DesktopWindow* parent_window);

		virtual const char*		GetWindowName()			{return "Add Custom Header Dialog";}

		virtual void			OnInit();
		virtual UINT32			OnOk();
		virtual void			OnCancel();

		OP_STATUS				GetHeaderName(OpString &header_name) { return m_edit->GetText(header_name) ;}

	private:
		OpEdit*					m_edit;
};

#endif //M2_SUPPORT
#endif //CUSTOMIZE_HEADER_DIALOG_H
