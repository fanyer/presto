/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl), Espen Sand
 */

#ifndef X11_UI_SETTINGS_H
#define X11_UI_SETTINGS_H

#include "platforms/quix/toolkits/ToolkitUiSettings.h"
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"

namespace ColorizeConversion
{
	void     ConvertRGBToHSV(uint32_t color, double& h, double& s, double& v);
	uint32_t ConvertHSVToRGB(double h, double s, double v);
	void     ColorizeHSV(double& h, double& s, double& v, double& colorize_h, double& colorize_s, double& colorize_v);
	uint32_t ColorizePixel(uint32_t color, double colorize_h, double colorize_s, double colorize_v);
	uint32_t ColorizeColor(uint32_t color, uint32_t colorize_color);
};

class X11UiSettings : public ToolkitUiSettings
{
private:
	struct ParsedColors
	{
		ParsedColors() { is_valid = false; scheme = 0; colorize_color = 0; }

		bool is_valid;
		uint32_t scheme;
		uint32_t colorize_color;

		uint32_t button;
		uint32_t button_light;
		uint32_t button_dark;
		uint32_t button_very_dark;
		uint32_t button_text;
		uint32_t background;
		uint32_t background_selected;
		uint32_t background_selected_nofocus;
		uint32_t background_highlighted;
		uint32_t background_highlighted_nofocus;
		uint32_t background_disabled;
		uint32_t text;
		uint32_t text_selected;
		uint32_t text_selected_nofocus;
		uint32_t text_highlighted;
		uint32_t text_highlighted_nofocus;
		uint32_t text_disabled;
		uint32_t scrollbar_background;
		uint32_t link;
		uint32_t visited_link;
		uint32_t ui_font;
		uint32_t ui_disabled_font;
		uint32_t ui_background;
		uint32_t ui_button_hover;
		uint32_t ui_menu;
		uint32_t skin;
		uint32_t tooltip_text;
		uint32_t tooltip_background;
		uint32_t workspace;
	};

public:
	// From ToolkitUiSettings
	virtual bool DefaultButtonOnRight() { return false; }

	virtual uint32_t GetSystemColor(SystemColor color);

	virtual bool GetDefaultFont(FontDetails& font) { return false; }

	virtual void GetFontRenderSettings(FontRenderSettings& settings) {}

	void SetColorScheme(uint32_t scheme, uint32_t colorize_color) 
		{ m_colors.is_valid = false; m_colors.scheme = scheme; m_colors.colorize_color = colorize_color; }

private:
	uint32_t Parse(PrefsCollectionFontsAndColors::customcolorpref key);
	static inline uint32_t MakeColor(uint32_t red, uint32_t green, uint32_t blue) { return (0xff << 24) | (red << 16) | (green << 8) | blue; }

private:
	ParsedColors m_colors;

};

#endif // X11_UI_SETTINGS_H
