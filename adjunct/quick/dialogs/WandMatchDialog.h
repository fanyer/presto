// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
//
// Copyright (C) 1995-2003 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Espen Sand
//

#ifndef __WAND_MATCH_DIALOG_H__
#define __WAND_MATCH_DIALOG_H__

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "modules/wand/wandmanager.h"
#include "modules/pi/OpWindow.h"

class WandMatchDialog : public Dialog
{
  public:
	~WandMatchDialog();
	OP_STATUS Init(DesktopWindow* parent_window, WandMatchInfo* info);
	void OnCancel();
	BOOL OnInputAction(OpInputAction* action);
	void OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);
	
	void UpdateList();
	
	const char*	GetWindowName()			{ return "Wand Match Dialog";}
	DialogType	GetDialogType()			{ return TYPE_OK_CANCEL; }
#ifdef VEGA_OPPAINTER_SUPPORT
	virtual BOOL			GetModality() {return FALSE;}
	virtual BOOL			GetOverlayed() {return TRUE;}
	virtual BOOL			GetDimPage() {return TRUE;}
#endif
private:
	WandMatchInfo* m_info;
};


#endif
