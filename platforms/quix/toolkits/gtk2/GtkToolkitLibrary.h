/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef GTK_TOOLKIT_LIBRARY_H
#define GTK_TOOLKIT_LIBRARY_H

#include "platforms/quix/toolkits/ToolkitLibrary.h"
#include "platforms/quix/toolkits/gtk2/GtkToolkitUiSettings.h"

#include <gtk/gtk.h>

class GtkWidgetPainter;

class GtkToolkitLibrary : public ToolkitLibrary
{
public:
	GtkToolkitLibrary();
	virtual ~GtkToolkitLibrary();
	virtual bool Init(X11Types::Display* display);
	virtual NativeSkinElement* GetNativeSkinElement(NativeSkinElement::NativeType type);
	virtual ToolkitUiSettings* GetUiSettings() { return m_settings; }
	virtual ToolkitWidgetPainter* GetWidgetPainter();
	virtual ToolkitColorChooser* CreateColorChooser();
	virtual ToolkitFileChooser* CreateFileChooser();
	virtual void SetMainloopRunner(ToolkitMainloopRunner* runner);
	virtual ToolkitPrinterIntegration* CreatePrinterIntegration();
	virtual bool IsStyleChanged();
	virtual bool BlockOperaInputOnDialogs() { return true; }
	virtual const char* ToolkitInformation();
	virtual void GetPopupMenuLayout(PopupMenuLayout& layout);
	virtual void SetColorScheme(uint32_t mode, uint32_t colorize_color) {}
	static void SetCanCallRunSlice(bool value);
	virtual void SetIsStyleChanged(bool is_style_changed) { m_is_style_changed = is_style_changed; }

private:
	static gboolean RunSlice(gpointer data);

	GtkWidget* m_window;
	GtkWidget* m_proto_layout;
	GtkToolkitUiSettings* m_settings;
	GtkStyle *current_style;
	guint m_timer_id;
	GtkWidgetPainter* m_widget_painter;
	char m_gtk_version[128];
	ToolkitMainloopRunner* m_runner;
	bool m_is_style_changed;

	static GtkToolkitLibrary* s_instance;
};

#endif // GTK_TOOLKIT_LIBRARY_H
