/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Bazyli Zygan bazyl@opera.com
 */

#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT

#include "platforms/mac/pi/desktop/MacGadgetAboutDialog.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/quick_toolkit/widgets/OpIcon.h"
#include "modules/gadgets/OpGadgetClass.h"
#include "adjunct/quick/managers/FavIconManager.h"

OP_STATUS MacGadgetAboutDialog::Init()
{
	Dialog::Init(g_application->GetActiveDesktopWindow());
	OpString val;
	
	// Set bolds, font sizes and stuff
	OpIcon  *icon   = static_cast<OpIcon*>(GetWidgetByName("gadget_icon"));
	OpLabel *title  = static_cast<OpLabel*>(GetWidgetByName("gadget_title_label"));
	OpLabel *author = static_cast<OpLabel*>(GetWidgetByName("gadget_author_label"));
	OpLabel *desc   = static_cast<OpLabel*>(GetWidgetByName("gadget_desc_label"));
	title->SetSystemFontWeight(QuickOpWidgetBase::WEIGHT_BOLD);
	title->SetRelativeSystemFontSize(140);
	//author->SetSystemFontWeight(QuickOpWidgetBase::WEIGHT_BOLD);
	
	// Set text values
	m_gadget_class->GetGadgetName(val);
	SetTitle(val);
	title->SetText(val.CStr());
	m_gadget_class->GetGadgetAuthor(val);
	author->SetText(val.CStr());
	m_gadget_class->GetGadgetDescription(val);
	desc->SetText(val.CStr());
	
	// Check if this widget has an Icon. If so - show it
	
    OpString icon_path;
    INT32 dummy_width, dummy_height;
    m_gadget_class->GetGadgetIcon(0, icon_path, dummy_width, 
								  dummy_height);
    FileImage file_image(icon_path, NULL);
    
	if (!(file_image.GetImage().IsEmpty()))
	{
		Image img = file_image.GetImage();
		icon->SetBitmapImage(img);
	}
	
	
	return OpStatus::OK;
}

MacGadgetAboutDialog::MacGadgetAboutDialog(OpGadgetClass *gadgetClass) 
: m_gadget_class(gadgetClass)
{
}


MacGadgetAboutDialog::~MacGadgetAboutDialog()
{
}



#endif // WIDGET_RUNTIME_SUPPORT
