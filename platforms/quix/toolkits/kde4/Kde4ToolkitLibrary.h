/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 * @author Rune Myrland (runem)
 */

#ifndef KDE4_TOOLKIT_LIBRARY_H
#define KDE4_TOOLKIT_LIBRARY_H

#include "platforms/quix/toolkits/ToolkitLibrary.h"
#include "platforms/quix/toolkits/ToolkitUiSettings.h"
#include <QString>
#include <stdio.h>

class KApplication;
class Kde4UiSettings;
class Kde4Mainloop;
class Kde4WidgetPainter;
class DummyWidget;

class Kde4ToolkitLibrary : public ToolkitLibrary
{
public:
	Kde4ToolkitLibrary();
	virtual ~Kde4ToolkitLibrary();

	virtual bool Init(X11Types::Display* display);

	virtual NativeSkinElement* GetNativeSkinElement(NativeSkinElement::NativeType type);

	virtual ToolkitUiSettings* GetUiSettings();

	virtual ToolkitWidgetPainter* GetWidgetPainter();

	virtual ToolkitColorChooser* CreateColorChooser();

	virtual ToolkitFileChooser* CreateFileChooser();

	virtual void SetMainloopRunner(ToolkitMainloopRunner* runner);

	virtual ToolkitPrinterIntegration* CreatePrinterIntegration();

	virtual void SetColorScheme(uint32_t mode, uint32_t colorize_color) {}

	virtual bool IsStyleChanged();

	virtual bool BlockOperaInputOnDialogs() { return true; }

	virtual const char* ToolkitInformation();

	virtual void GetMenuBarLayout(MenuBarLayout& layout);

	virtual void GetPopupMenuLayout(PopupMenuLayout& layout);

private:
    KApplication* m_app;
	Kde4UiSettings* m_settings;
	Kde4Mainloop* m_loop;
	Kde4WidgetPainter* m_widget_painter;
	char m_kde_version[128];
	QString m_stylestring;
	DummyWidget* m_dummywidget;
};

#endif // GTK_TOOLKIT_LIBRARY_H
