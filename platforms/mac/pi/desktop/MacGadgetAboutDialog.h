/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Bazyli Zygan bazyl@opera.com
 */

#ifndef MAC_GADGET_ABOUT_DIALOG_H
#define MAC_GADGET_ABOUT_DIALOG_H

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"

#include "modules/widgets/OpButton.h"

class OpGadgetClass;

class MacGadgetAboutDialog
: public Dialog
{
public:
	
	MacGadgetAboutDialog(OpGadgetClass *gadgetClass);
	~MacGadgetAboutDialog();
	
	/**
	 * Using DIALOG_TYPE_WIDGET_INSTALLER, we don't need new one.
	 */
	DialogType GetDialogType() { return TYPE_OK; }
	virtual const char* GetWindowName() { return "Mac Gadget About Dialog"; }
	//		virtual Type GetType() { return DIALOG_TYPE_WIDGET_UNINSTALLER; }
	
	/**
	 * Dialog initialization.
	 *
	 * @param src_wgt_path Path to widget wgt file.
	 * @return Error status.
	 */
	OP_STATUS Init();
	
private:
	
	OpGadgetClass *m_gadget_class;
};

#endif // WIDGET_RUNTIME_SUPPORT
#endif // MAC_GADGET_ABOUT_DIALOG_H