/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
** 
*/

#include "core/pch.h"

#include "WindowsOpColorChooser.h"
#include "platforms/windows/pi/WindowsOpWindow.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"

OP_STATUS OpColorChooser::Create(OpColorChooser** new_object)
{
	OP_ASSERT(new_object != NULL);
	*new_object = new WindowsOpColorChooser();
	return *new_object ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

WindowsOpColorChooser::WindowsOpColorChooser()
{
}

WindowsOpColorChooser::~WindowsOpColorChooser()
{
}

OP_STATUS WindowsOpColorChooser::Show(COLORREF initial_color, OpColorChooserListener* listener, DesktopWindow* parent)
{
    // Color variables
    CHOOSECOLORA         cc;
    static COLORREF      custom[16];

	// Convert from core COLORREF to windows COLORREF
	initial_color = RGB(GetRValue(initial_color), GetGValue(initial_color), GetBValue(initial_color));
	
	for (int i = 0; i < 16; i++)
        custom[i]  = RGB(255, 255, 255);
	
	custom[0] = initial_color;
	
    // Set all structure fields to zero.
	memset(&cc, 0, sizeof(CHOOSECOLORA));
	
    // Initialize the necessary CHOOSECOLOR members.
    cc.lStructSize       = sizeof(CHOOSECOLORA);
    cc.hwndOwner         = ((WindowsOpWindow*)(parent->GetWindow()))->GetDesktopParent();
    cc.hInstance         = 0;
    cc.rgbResult         = initial_color;
    cc.lpCustColors      = custom;
    cc.Flags             = CC_RGBINIT | CC_FULLOPEN;
    cc.lCustData         = 0;
    cc.lpfnHook          = NULL;
    cc.lpTemplateName    = NULL;
	
    if (!ChooseColorA(&cc))
        return OpStatus::OK;
	
    // SELECTED RGB COLOR
    listener->OnColorSelected(cc.rgbResult);
	
	return OpStatus::OK;
}

