/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef X11_TOOLKIT_LIBRARY_H
#define X11_TOOLKIT_LIBRARY_H

#include "platforms/quix/toolkits/ToolkitLibrary.h"
#include "platforms/quix/toolkits/x11/X11UiSettings.h"
#include <stdio.h>

class X11ToolkitLibrary : public ToolkitLibrary
{
public:
	virtual ~X11ToolkitLibrary() {}

	// From ToolkitLIbrary
	virtual bool Init(X11Types::Display* display) { return true; }

	virtual NativeSkinElement* GetNativeSkinElement(NativeSkinElement::NativeType type) { return NULL; }

	virtual ToolkitWidgetPainter* GetWidgetPainter() { return NULL; }

	virtual ToolkitUiSettings* GetUiSettings();

	virtual ToolkitColorChooser* CreateColorChooser();

	virtual ToolkitFileChooser* CreateFileChooser();

	virtual void SetMainloopRunner(ToolkitMainloopRunner* runner) {}

	virtual ToolkitPrinterIntegration* CreatePrinterIntegration();

	virtual bool IsStyleChanged() { return false; }

	virtual bool BlockOperaInputOnDialogs() { return false; }

	virtual const char* ToolkitInformation() { return "x11"; }

	virtual void SetColorScheme(uint32_t mode, uint32_t colorize_color) { m_ui_settings.SetColorScheme(mode, colorize_color); }

	virtual void GetPopupMenuLayout(PopupMenuLayout& layout) {layout.arrow_overlap = 3; layout.text_margin = 12; }

	virtual bool HasToolkitSkinning() { return false; }

private:
	X11UiSettings m_ui_settings;
};

#endif // X11_TOOLKIT_LIBRARY_H
