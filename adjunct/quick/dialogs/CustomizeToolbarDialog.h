/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef CUSTOMIZETOOLBAR_H
#define CUSTOMIZETOOLBAR_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/desktop_util/treemodel/simpletreemodel.h"
#include "adjunct/quick_toolkit/widgets/OpBrowserView.h"

#define CUSTOMIZE_PAGE_SKIN		0
#define CUSTOMIZE_PAGE_PANELS	1
#define CUSTOMIZE_PAGE_TOOLBARS 2
#define CUSTOMIZE_PAGE_BUTTONS	3
//#define CUSTOMIZE_PAGE_ADVANCED		4

/***********************************************************************************
**
**	CustomizeToolbarDialog
**
***********************************************************************************/

class CustomizeToolbarDialog : public Dialog, public OpPageListener
{
	public:
			
								CustomizeToolbarDialog();
		virtual					~CustomizeToolbarDialog();

		DialogType				GetDialogType()			{return TYPE_PROPERTIES;}
		Type					GetType()				{return DIALOG_TYPE_CUSTOMIZE_TOOLBAR;}
		const char*				GetWindowName()			{return "Customize Toolbar Dialog";}
		BOOL					GetModality()			{return FALSE;}
		const char*				GetHelpAnchor()			{return ""; /* "customtoolbar.html"; Problem: when customize dialog is open, other windows allow editing of toolbars */}

		OpToolbar*				GoToCustomToolbar();

		OpToolbar*				GetTargetToolbar()		{return m_target_toolbar;}
		void					SetTargetToolbar(OpToolbar* target_toolbar);

		BOOL					GetShowHiddenToolbarsWhileCustomizing() {return m_show_hidden_toolbars_while_customizing;}
		void					SetShowHiddenToolbarsWhileCustomizing(BOOL show);

		virtual void			OnInit();
	    virtual void            OnChange(OpWidget *widget, BOOL changed_by_mouse = FALSE);
		virtual UINT32			OnOk();
		virtual void			OnCancel();
		virtual void			OnSettingsChanged(DesktopSettings* settings);
		virtual void			OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);
		
		BOOL					OnInputAction(OpInputAction* action);

		BOOL					OnPagePopupMenu(OpWindowCommander* commander, OpDocumentContext& context);
	private:

		void					AddToolbar(const uni_char* title, const char* name, BOOL locked = TRUE);
		void					ApplyNewSelectedSkin(OpTreeView *skin_configurations);

		OpTreeView*				m_treeview;
		TemplateTreeModel<OpToolbar> m_treeview_model;
		OpToolbar*				m_target_toolbar;
		OpToolbar*				m_defaults_toolbar;
		BOOL					m_show_hidden_toolbars_while_customizing;
		OpFile*					m_old_skin;
		INT32					m_old_skin_scale;
		INT32					m_old_color_scheme_mode;
		INT32					m_old_color_scheme_color;
		BOOL					m_old_special_effects;
		BOOL					m_skin_changed;
		BOOL					m_is_initiating;
};

#endif
