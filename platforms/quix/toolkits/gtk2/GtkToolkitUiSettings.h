/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef GTK_TOOLKIT_UI_SETTINGS
#define GTK_TOOLKIT_UI_SETTINGS

#include "platforms/quix/toolkits/ToolkitUiSettings.h"

#include <gtk/gtk.h>

class GtkToolkitUiSettings : public ToolkitUiSettings
{
public:
	GtkToolkitUiSettings() : m_layout(0), m_menu_item(0), m_style(0), m_settings(0) {}
	~GtkToolkitUiSettings();

	/** Initialize this settings object
	  * @return Whether initialization was successful
	  */
	bool Init(GtkWidget* layout);

	void ChangeStyle(GtkStyle* style); 

	// From ToolkitUiSettings
	virtual bool DefaultButtonOnRight();

	virtual uint32_t GetSystemColor(SystemColor color);

	virtual bool GetDefaultFont(FontDetails& font);

	virtual void GetFontRenderSettings(FontRenderSettings& settings);

	void GetThemeName(char *theme_name);

private:
	inline uint32_t ConvertColor(const GdkColor& color) { return MakeColor(color.red >> 8, color.green >> 8, color.blue >> 8); }
	inline uint32_t MakeColor(uint8_t red, uint8_t green, uint8_t blue) { return (0xff << 24) | ((uint32_t)red << 16) | ((uint32_t)green << 8) | ((uint32_t)blue); }

	GtkWidget*   m_layout;
	GtkWidget*   m_menu_item;
	GtkStyle*    m_style;
	GtkSettings* m_settings;
};

#endif // GTK_TOOLKIT_UI_SETTINGS
