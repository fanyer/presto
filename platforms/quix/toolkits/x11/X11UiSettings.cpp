/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl), Espen Sand
 */

#include "core/pch.h"
#include "platforms/quix/toolkits/x11/X11UiSettings.h"


void ColorizeConversion::ConvertRGBToHSV(uint32_t color, double& h, double& s, double& v)
{
	//convert p to range [0,1]
	double r = ((color >> 16) & 0xff) / 255.0;
	double g = ((color >> 8) & 0xff) / 255.0;
	double b = (color & 0xff) / 255.0;

	// max of r, g, b
	double tmp = r > g ? r : g;
	double Max = tmp > b ? tmp : b;
	// min of r, g, b
	tmp = r > g ? g : r;
	double Min = tmp > b ? b : tmp;

	// set value/brightness v
	v = Max;

	// calculate saturation s
	if (Max != 0)
		s = (Max - Min) / Max;
	else
		s = 0;

	// calculate hue h
	if (s == 0)
		h = 0; // UNDEFINED!!
	else
	{
		double delta = Max - Min;
		if (r == Max)
			h = (g - b) / delta;			// resulting color is between yellow and magenta
		else if (g == Max)
			h = 2 + (b - r) / delta;		// resulting color is between cyan and yellow
		else if (b == Max)
			h = 4 + (r - g) / delta;		// resulting color is between magenta and cyan

		h *= 60;							// convert hue to degrees

		if (h < 0)							// make sure positive
			h += 360;
	}
}


uint32_t ColorizeConversion::ConvertHSVToRGB(double h, double s, double v)
{
	double R, G, B;

	R = G = B = 0;

	if (s == 0)
	{
		R = v;
		G = v;
		B = v;
	}
	else
	{
		int i;
		double f, p, q, t;

		if (h == 360)
			h = 0;
		h /= 60;			// h now in [0,6)
		i = int(floor(h));
		f = h - i;
		p = v * (1 - s);
		q = v * (1 - (s * f));
		t = v * (1 - (s * (1 - f)));

		switch (i)
		{
			case 0: R = v;
				    G = t;
					B = p;
					break;
			case 1: R = q;
				    G = v;
					B = p;
					break;
			case 2: R = p;
				    G = v;
					B = t;
					break;
			case 3: R = p;
				    G = q;
					B = v;
					break;
			case 4: R = t;
				    G = p;
					B = v;
					break;
			case 5: R = v;
				    G = p;
					B = q;
		}
	}

	uint8_t r = (uint8_t)(R * 255);
	uint8_t g = (uint8_t)(G * 255);
	uint8_t b = (uint8_t)(B * 255);

	// don't set any alpha value
	return (r << 16) | (g << 8) | b;
}



void ColorizeConversion::ColorizeHSV(double& h, double& s, double& v, double& colorize_h, double& colorize_s, double& colorize_v)
{
	if (colorize_s == 0)
	{
		s = 0;
	}
	else
	{
		h = colorize_h;
		if (colorize_s > s)
		{
			s += (colorize_s - s) * (1 - v);

			if (s < 0)
				s = 0;
			else if (s > 1)
				s = 1;
		}
		else
			s = colorize_s;
	}
}

uint32_t ColorizeConversion::ColorizePixel(uint32_t color, double colorize_h, double colorize_s, double colorize_v)
{
	double h, s, v;

	ConvertRGBToHSV(color, h, s, v);

	ColorizeHSV(h, s, v, colorize_h, colorize_s,  colorize_v);

	return (color & 0xff000000) | ConvertHSVToRGB(h, s, v);
}


uint32_t ColorizeConversion::ColorizeColor(uint32_t color, uint32_t colorize_color)
{
	// convert colorref to ARGB pixel (but remove the alpha)

	uint32_t colorize_color_rgb = (colorize_color & OP_RGBA(0, 0, 0, 255)) | ((colorize_color & 0xff) << 16) | (colorize_color & 0xff00) | ((colorize_color >> 16) & 0xff);
	uint32_t color_rgb = (color & OP_RGBA(0, 0, 0, 255)) | ((color & 0xff) << 16) | (color & 0xff00) | ((color >> 16) & 0xff);

	double h, s, v;

	ConvertRGBToHSV(colorize_color_rgb, h, s, v);
	color_rgb = ColorizePixel(color_rgb, h, s, v);
	
	color = (color_rgb & OP_RGBA(0, 0, 0, 255)) | ((color_rgb & 0xff) << 16) | (color_rgb & 0xff00) | ((color_rgb >> 16) & 0xff);

	return 0xFF000000 | color;
}



uint32_t X11UiSettings::GetSystemColor(SystemColor color)
{
	if (!m_colors.is_valid)
	{
		m_colors.is_valid = TRUE;
		m_colors.button = Parse(PrefsCollectionFontsAndColors::Button);
		m_colors.button_light = Parse(PrefsCollectionFontsAndColors::ButtonLight);
		m_colors.button_dark = Parse(PrefsCollectionFontsAndColors::ButtonDark);
		m_colors.button_very_dark = Parse(PrefsCollectionFontsAndColors::ButtonVeryDark);
		m_colors.button_text = Parse(PrefsCollectionFontsAndColors::ButtonText);
		m_colors.background = Parse(PrefsCollectionFontsAndColors::Background);
		m_colors.background_selected = Parse(PrefsCollectionFontsAndColors::BackgroundSelected);
		m_colors.background_selected_nofocus = Parse(PrefsCollectionFontsAndColors::BackgroundSelectedNoFocus);
		m_colors.background_highlighted = Parse(PrefsCollectionFontsAndColors::BackgroundHighlighted);
		m_colors.background_highlighted_nofocus = Parse(PrefsCollectionFontsAndColors::BackgroundHighlightedNoFocus);
		m_colors.background_disabled = Parse(PrefsCollectionFontsAndColors::BackgroundDisabled);
		m_colors.text = Parse(PrefsCollectionFontsAndColors::Text);
		m_colors.text_selected = Parse(PrefsCollectionFontsAndColors::TextSelected);
		m_colors.text_selected_nofocus = Parse(PrefsCollectionFontsAndColors::TextSelectedNoFocus);
		m_colors.text_highlighted = Parse(PrefsCollectionFontsAndColors::TextHighlighted);
		m_colors.text_highlighted_nofocus = Parse(PrefsCollectionFontsAndColors::TextHighlightedNoFocus);
		m_colors.text_disabled = Parse(PrefsCollectionFontsAndColors::TextDisabled);
		m_colors.scrollbar_background = Parse(PrefsCollectionFontsAndColors::ScrollbarBackground);
		m_colors.link = Parse(PrefsCollectionFontsAndColors::Link);
		m_colors.visited_link = Parse(PrefsCollectionFontsAndColors::VisitedLink);
		m_colors.ui_font = Parse(PrefsCollectionFontsAndColors::UIFont);
		m_colors.ui_disabled_font = Parse(PrefsCollectionFontsAndColors::DisabledUIFont);
		m_colors.ui_background = Parse(PrefsCollectionFontsAndColors::UIBackground);
		m_colors.ui_button_hover = Parse(PrefsCollectionFontsAndColors::UIButtonHover);
		m_colors.ui_menu = Parse(PrefsCollectionFontsAndColors::UIMenu);
		m_colors.skin = Parse(PrefsCollectionFontsAndColors::Skin);
		m_colors.tooltip_text = Parse(PrefsCollectionFontsAndColors::TooltipText);
		m_colors.tooltip_background = Parse(PrefsCollectionFontsAndColors::TooltipBackground);
		m_colors.workspace = Parse(PrefsCollectionFontsAndColors::Workspace);

		if (m_colors.scheme == 1 || m_colors.scheme == 2)
		{
			uint32_t colorize_color = m_colors.scheme == 1 ? m_colors.skin : m_colors.colorize_color;

			m_colors.button               = ColorizeConversion::ColorizeColor(m_colors.ui_background, colorize_color);
			m_colors.button_light         = ColorizeConversion::ColorizeColor(m_colors.button_light, colorize_color);
			m_colors.button_dark          = ColorizeConversion::ColorizeColor(m_colors.button_dark, colorize_color);
			m_colors.button_very_dark     = ColorizeConversion::ColorizeColor(m_colors.button_very_dark, colorize_color);
			m_colors.ui_background        = ColorizeConversion::ColorizeColor(m_colors.ui_background, colorize_color);
			m_colors.background_disabled  = ColorizeConversion::ColorizeColor(m_colors.background_disabled, colorize_color);
			m_colors.scrollbar_background = ColorizeConversion::ColorizeColor(m_colors.scrollbar_background, colorize_color);
			m_colors.ui_background        = ColorizeConversion::ColorizeColor(m_colors.ui_background, colorize_color);
			m_colors.ui_button_hover      = ColorizeConversion::ColorizeColor(m_colors.ui_button_hover, colorize_color);
			m_colors.workspace            = ColorizeConversion::ColorizeColor(m_colors.workspace, colorize_color);
		}
	}

	switch (color)
	{
		case COLOR_BUTTON:
			return m_colors.button;
		case COLOR_BUTTON_LIGHT:
			return m_colors.button_light;
		case COLOR_BUTTON_DARK:
			return m_colors.button_dark;
		case COLOR_BUTTON_VERYDARK:
			return m_colors.button_very_dark;
		case COLOR_BUTTON_TEXT:
			return m_colors.button_text;
		case COLOR_BACKGROUND:
			return m_colors.background;
		case COLOR_BACKGROUND_SELECTED:
			return m_colors.background_selected;
		case COLOR_BACKGROUND_SELECTED_NOFOCUS:
			return m_colors.background_selected_nofocus;
		case COLOR_BACKGROUND_HIGHLIGHTED:
			return m_colors.background_highlighted;
		case COLOR_BACKGROUND_HIGHLIGHTED_NOFOCUS:
			return m_colors.background_highlighted_nofocus;
		case COLOR_BACKGROUND_DISABLED:
			return m_colors.background_disabled;
		case COLOR_TEXT:
			return m_colors.text;
		case COLOR_TEXT_SELECTED:
			return m_colors.text_selected;
		case COLOR_TEXT_SELECTED_NOFOCUS:
			return m_colors.text_selected_nofocus;
		case COLOR_TEXT_HIGHLIGHTED:
			return m_colors.text_highlighted;
		case COLOR_TEXT_HIGHLIGHTED_NOFOCUS:
			return m_colors.text_highlighted_nofocus;
		case COLOR_TEXT_DISABLED:
			return m_colors.text_disabled;
		case COLOR_SCROLLBAR_BACKGROUND:
			return m_colors.scrollbar_background;
		case COLOR_LINK:
			return m_colors.link;
		case COLOR_VISITED_LINK:
			return m_colors.visited_link;
		case COLOR_UI_FONT:
			return m_colors.ui_font;
		case COLOR_UI_DISABLED_FONT:
			return m_colors.ui_disabled_font;
		case COLOR_UI_BACKGROUND:
			return m_colors.ui_background;
		case COLOR_UI_BUTTON_HOVER:
			return m_colors.ui_button_hover;
		case COLOR_UI_MENU:
			return m_colors.ui_menu;
		case COLOR_SKIN:
			return m_colors.skin;
		case COLOR_TOOLTIP_TEXT:
			return m_colors.tooltip_text;
		case COLOR_TOOLTIP_BACKGROUND:
			return m_colors.tooltip_background;
		case COLOR_WORKSPACE:
			return m_colors.workspace;
		default:
#ifdef DEBUG
			printf("opera: Unknown color code %d. Using red.\n", color); 
			return MakeColor(0xff, 0, 0);
#else
			return MakeColor(0xff, 0xff, 0xff); 
#endif
	}
}

uint32_t X11UiSettings::Parse(PrefsCollectionFontsAndColors::customcolorpref key)
{
	// Prefs gives us BGR, convert to RGB as that is assumed by X11OpUiInfo which handles colors from all tookits
	UINT32 col = g_pcfontscolors->GetColor(key);
	return 0xFF000000 | (OP_GET_R_VALUE(col) << 16) | (OP_GET_G_VALUE(col) << 8) | OP_GET_B_VALUE(col);
}
