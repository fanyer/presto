/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "platforms/quix/toolkits/gtk2/GtkToolkitUiSettings.h"

#include "platforms/quix/toolkits/gtk2/GtkPorting.h"
#include <fontconfig/fontconfig.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

GtkToolkitUiSettings::~GtkToolkitUiSettings()
{
	if (m_menu_item)
		gtk_widget_destroy(m_menu_item);
}

bool GtkToolkitUiSettings::Init(GtkWidget* layout)
{
	m_layout = layout;

	m_menu_item = gtk_menu_item_new();
	gtk_container_add(GTK_CONTAINER(m_layout), m_menu_item);
	
	m_settings = gtk_settings_get_default();

	return m_settings;
}

void GtkToolkitUiSettings::ChangeStyle(GtkStyle *style)
{ 
	m_style = style;
}

bool GtkToolkitUiSettings::DefaultButtonOnRight()
{
	gboolean alternative_order;
	g_object_get(m_settings, "gtk-alternative-button-order", &alternative_order, NULL);
	return !alternative_order;
}

uint32_t GtkToolkitUiSettings::GetSystemColor(SystemColor color)
{
	switch (color)
	{
		case COLOR_BUTTON:
		case COLOR_SCROLLBAR_BACKGROUND:
			return ConvertColor(m_style->bg[GTK_STATE_NORMAL]);
		case COLOR_BUTTON_LIGHT:
			return ConvertColor(m_style->light[GTK_STATE_NORMAL]);
		case COLOR_BUTTON_DARK:
			return ConvertColor(m_style->dark[GTK_STATE_NORMAL]);
		case COLOR_BUTTON_VERYDARK:
			return ConvertColor(m_style->black);
		case COLOR_UI_BACKGROUND:
			return ConvertColor(m_style->bg[GTK_STATE_NORMAL]);
		case COLOR_BACKGROUND:
		case COLOR_SKIN:
			return ConvertColor(m_style->base[GTK_STATE_NORMAL]);
		case COLOR_BACKGROUND_SELECTED:
		case COLOR_BACKGROUND_SELECTED_NOFOCUS:
			return ConvertColor(m_style->base[GTK_STATE_SELECTED]);
		case COLOR_BACKGROUND_HIGHLIGHTED:
			return MakeColor(35, 220, 35); //Green
		case COLOR_BACKGROUND_HIGHLIGHTED_NOFOCUS:
			return MakeColor(255, 225, 35); //Yellow
		case COLOR_BACKGROUND_DISABLED:
			return ConvertColor(m_style->base[GTK_STATE_INSENSITIVE]);
		case COLOR_BUTTON_TEXT:
			return ConvertColor(m_style->fg[GTK_STATE_NORMAL]);
		case COLOR_TEXT:
			return ConvertColor(m_style->text[GTK_STATE_NORMAL]);
		case COLOR_UI_FONT:
			return ConvertColor(m_style->fg[GTK_STATE_NORMAL]);
		case COLOR_TEXT_SELECTED:
		case COLOR_TEXT_SELECTED_NOFOCUS:
		case COLOR_TEXT_HIGHLIGHTED:
		case COLOR_TEXT_HIGHLIGHTED_NOFOCUS:
			return ConvertColor(m_style->fg[GTK_STATE_SELECTED]);
		case COLOR_TEXT_DISABLED:
		case COLOR_UI_DISABLED_FONT:
			return ConvertColor(m_style->fg[GTK_STATE_INSENSITIVE]);
		case COLOR_UI_BUTTON_HOVER:
			return ConvertColor(m_style->bg[GTK_STATE_PRELIGHT]);
		case COLOR_LINK:
			return MakeColor(0, 0, 204);
		case COLOR_VISITED_LINK:
			return MakeColor(128, 0, 128);
		case COLOR_TOOLTIP_BACKGROUND:
			return MakeColor(0xFF, 0xFF, 0xE1);
		case COLOR_TOOLTIP_TEXT:
			return MakeColor(0, 0, 0);
		case COLOR_UI_MENU:
		{
			GtkStyle* style = gtk_style_attach(
					gtk_widget_get_style(m_menu_item),
					gtk_widget_get_parent_window(m_menu_item));
			uint32_t color = ConvertColor(style->fg[GTK_STATE_NORMAL]);
			gtk_style_detach(style);
			return color;
		}
		case COLOR_WORKSPACE:
			return MakeColor(0x80, 0x80, 0x80);
		default:
#ifdef DEBUG
			printf("GtkToolkitUiSettings::GetSystemColor: Unknown color code %d. Using red.\n", color);
			return MakeColor(0xff, 0, 0);
#else
			return MakeColor(0xff, 0xff, 0xff);
#endif
	}
}

bool GtkToolkitUiSettings::GetDefaultFont(FontDetails& font)
{
	gchararray font_face = 0;
	g_object_get(m_settings, "gtk-font-name", &font_face, NULL);
	PangoFontDescription* font_desc = pango_font_description_from_string(font_face);
	g_free(font_face);
	if (!font_desc)
		return false; 

	const char* family = pango_font_description_get_family(font_desc);

	if (family)
	{
		if (strcmp(family, "Sans") == 0)
			font.type = SANSSERIF;
		else if (strcmp(family, "Serif") == 0)
			font.type = SERIF;
		else if (strcmp(family, "Monospace") == 0)
			font.type = MONOSPACE;

		font.family = strdup(family);
	}
	font.weight = pango_font_description_get_weight(font_desc) / 100;
	font.italic = pango_font_description_get_style(font_desc) == PANGO_STYLE_ITALIC;
	font.smallcaps = pango_font_description_get_variant(font_desc) == PANGO_VARIANT_SMALL_CAPS;
	
	double size = pango_font_description_get_size(font_desc) / PANGO_SCALE;
	font.size = size;

	pango_font_description_free(font_desc);

	return true;
}

void GtkToolkitUiSettings::GetFontRenderSettings(FontRenderSettings& settings)
{
	gint antialias;
	gint hinting;
	FcChar8* hintstyle;
	FcChar8* rgba;

	g_object_get(m_settings,
				 "gtk-xft-antialias", &antialias,
				 "gtk-xft-hinting", &hinting,
				 "gtk-xft-hintstyle", &hintstyle,
				 "gtk-xft-rgba", &rgba, NULL);

	if (antialias != -1)
		settings.antialias = antialias;
	if (hinting != -1)
		settings.hinting = hinting;
	if (hintstyle)
		settings.hintstyle = FcNameConstant(hintstyle, &settings.hintstyle) ? settings.hintstyle : USE_FONTCONFIG;
	if (rgba)
		settings.rgba = FcNameConstant(rgba, &settings.rgba) ? settings.rgba : USE_FONTCONFIG;

	g_free(hintstyle);
	g_free(rgba);
}

void GtkToolkitUiSettings::GetThemeName(char *theme_name)
{
	gchar* name;
	g_object_get(m_settings, "gtk-theme-name", &name, NULL);
	snprintf(theme_name, 64, "%s", name);
	g_free(name);
}

