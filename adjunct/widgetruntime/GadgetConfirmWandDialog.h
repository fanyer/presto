/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef GADGET_CONFIRM_WAND_DIALOG_H
#define GADGET_CONFIRM_WAND_DIALOG_H

#ifdef WIDGET_RUNTIME_SUPPORT

#include "adjunct/quick_toolkit/widgets/Dialog.h"

class WandInfo;

class GadgetConfirmWandDialog : public Dialog
{
public:
	OP_STATUS Init(DesktopWindow* parent_window, WandInfo* info);

	void OnInit();
	
	INT32 GetButtonCount() {return 3;}

	void GetButtonInfo(INT32 index, OpInputAction*& action, OpString& text, BOOL& is_enabled, BOOL& is_default, OpString8& name);

	UINT32 OnOk();

	void OnCancel();
		
	BOOL OnInputAction(OpInputAction* action);

	const char*	GetWindowName()			{ return "Wand Widget Dialog";}
	DialogType	GetDialogType()			{ return TYPE_OK_CANCEL; }
private:
	WandInfo* m_info;
};

#endif // WIDGET_RUNTIME_SUPPORT

#endif // GADGET_CONFIRM_WAND_DIALOG_H
