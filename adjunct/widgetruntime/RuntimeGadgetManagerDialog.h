/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Tomasz Kupczyk tkupczyk@opera.com
 */

#ifndef RUNTIME_GADGET_MANAGER_DIALOG_H
#define RUNTIME_GADGET_MANAGER_DIALOG_H

#ifdef WIDGET_RUNTIME_SUPPORT
#ifdef _UNIX_DESKTOP_

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/widgetruntime/models/GadgetsTreeModel.h"
#include "modules/locale/locale-enum.h"

class OpTreeView;

/**
 *
 */
class RuntimeGadgetManagerDialog
	: public Dialog
{
	public:

		RuntimeGadgetManagerDialog();

		DialogType GetDialogType() { return TYPE_OK; }

		virtual const char* GetWindowName() 
			{ return "Runtime Gadget Manager Dialog"; }

		virtual Type GetType() { return DIALOG_TYPE_WIDGET_UNINSTALLER; }

		/**
		 * Dialog initialization.
		 */
		OP_STATUS Init();

		Str::LocaleString GetOkTextID() { return Str::MI_IDM_Exit; }

		// == OpWidgetListener ======================

		virtual void OnChange(OpWidget *widget, BOOL changed_by_mouse);

		virtual DesktopWindow *GetDesktopWindow() { return this; }

	private:

		BOOL OnInputAction(OpInputAction *action);

		GadgetsTreeModel m_model;

		OpTreeView *m_view;

		/**
		 * Tries to uninstall gadget, that is selected in list.
		 * Does not update dialog widgets, so after using this
		 * method list content may not be synchronised with gadgets
		 * actually installed.
		 * 
		 * @return status
		 * @retval OpStatus::OK uninstallation was succesful
		 * @retval OpStatus::Err uninstallation failed or no item was selected
		 *         in list.
		 */
		OP_STATUS RemoveSelectedGadget();

		void ClearWidgetLabels();

		static const char* const MANAGER_ABOUT_LABEL;
		static const char* const WIDGETLIST;
		static const char* const UNINSTALL_BUTTON;
		static const char* const WIDGET_NAME_LABEL;
		static const char* const AUTHOR_NAME_LABEL;
		static const char* const WIDGET_DESC_LABEL;

		static const UINT32 GADGET_ICON_SIZE = 32;
		static const UINT32 SEPARATOR_COLUMN_WIDTH = 5;

		enum 
		{
			ICON_COLUMN = 0,
			SEPARATOR_COLUMN,
			NAME_COLUMN
		};
};

#endif // _UNIX_DESKTOP_
#endif // WIDGET_RUNTIME_SUPPORT
#endif // RUNTIME_GADGET_MANAGER_DIALOG_H
