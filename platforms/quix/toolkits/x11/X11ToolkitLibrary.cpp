/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl) and Espen Sand
 */

#include "core/pch.h"

#include "platforms/quix/toolkits/x11/X11ToolkitLibrary.h"
#include "platforms/quix/toolkits/x11/X11ToolkitColorChooser.h"
#include "platforms/quix/toolkits/x11/X11FileChooser.h"
#include "platforms/quix/toolkits/x11/X11PrinterChooser.h"


ToolkitUiSettings* X11ToolkitLibrary::GetUiSettings()
{
	return &m_ui_settings;
}

ToolkitFileChooser* X11ToolkitLibrary::CreateFileChooser() 
{ 
	return new X11ToolkitFileChooser();
}

ToolkitPrinterIntegration* X11ToolkitLibrary::CreatePrinterIntegration() 
{
	return new X11ToolkitPrinterIntegration();
}

ToolkitColorChooser* X11ToolkitLibrary::CreateColorChooser() 
{ 
	return new X11ToolkitColorChooser();
}
