/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef TOOLKIT_UI_SETTINGS_H
#define TOOLKIT_UI_SETTINGS_H

#include <stdint.h>
#include <stdlib.h>

class ToolkitUiSettings
{
public:
	enum SystemColor
	{
		COLOR_BUTTON,
		COLOR_BUTTON_LIGHT,
		COLOR_BUTTON_DARK,
		COLOR_BUTTON_VERYDARK,
		COLOR_BUTTON_TEXT,
		COLOR_BACKGROUND,
		COLOR_BACKGROUND_SELECTED,
		COLOR_BACKGROUND_SELECTED_NOFOCUS,
		COLOR_BACKGROUND_DISABLED,
		COLOR_TEXT,
		COLOR_TEXT_SELECTED,
		COLOR_TEXT_SELECTED_NOFOCUS,
		COLOR_TEXT_DISABLED,
		COLOR_TEXT_HIGHLIGHTED,
		COLOR_TEXT_HIGHLIGHTED_NOFOCUS,
		COLOR_BACKGROUND_HIGHLIGHTED,
		COLOR_BACKGROUND_HIGHLIGHTED_NOFOCUS,
		COLOR_SCROLLBAR_BACKGROUND,
		COLOR_UI_BUTTON_HOVER,
		COLOR_UI_FONT,
		COLOR_UI_BACKGROUND,
		COLOR_UI_DISABLED_FONT,
		COLOR_SKIN,
		COLOR_LINK,
		COLOR_VISITED_LINK,
		COLOR_TOOLTIP_BACKGROUND,
		COLOR_TOOLTIP_TEXT,
		COLOR_UI_MENU,
		COLOR_WORKSPACE,

		COLOR_COUNT
	};
	
	/** @return Whether the button order should be such that the default button
	  * is on the right-hand side
	  */
	virtual bool DefaultButtonOnRight() = 0;

	/** Get the default system color
	  * @param color Color to get (see also OpUiInfo, on which these colors are based)
	  * @return A color in ARGB format
	  */
	virtual uint32_t GetSystemColor(SystemColor color) = 0;

	enum GenericFontType
	{
		SANSSERIF,
		SERIF,
		MONOSPACE
	};
	
	struct FontDetails
	{
		char* family;	///< Family name, should be allocated with malloc
		GenericFontType type; ///< Type to use if a lookup of family fails
		int weight;		///< Weight from 1-9, 4 is normal, 7 is bold
		bool italic;	///< Whether font is italic
		bool smallcaps;	///< Whether font should be displayed in smallcaps
		int size;		///< Font size, usually font height

		FontDetails() : family(0), type(SANSSERIF), weight(4), italic(false), smallcaps(false), size(10) {}
		~FontDetails() { free(family); }
	};

	/** Get the default font to use in the UI
	  * @return Whether the request was successful
	  */
	virtual bool GetDefaultFont(FontDetails& font) = 0;

	/** These font rendering settings match standard xft/fontconfig properties
	  * See http://fontconfig.org/fontconfig-devel/x19.html
	  */
	enum { USE_FONTCONFIG = -1 };
	struct FontRenderSettings
	{
		// setting:		// fontconfig property:
		int rgba;		/// FC_RGBA
		int hintstyle;	/// FC_HINT_STYLE
		int lcd_filter; /// FC_LCD_FILTER
		int antialias;	/// FC_ANTIALIAS
		int hinting;	/// FC_HINTING
		int autohint;	/// FC_AUTOHINT

		FontRenderSettings()
		  : rgba(USE_FONTCONFIG)
		  , hintstyle(USE_FONTCONFIG)
		  , lcd_filter(USE_FONTCONFIG)
		  , antialias(USE_FONTCONFIG)
		  , hinting(USE_FONTCONFIG)
		  , autohint(USE_FONTCONFIG)
		{}
	};

	/** Get font rendering settings for this toolkit
	  * @param settings Change settings to a value other than USE_FONTCONFIG to use it directly
	  */
	virtual void GetFontRenderSettings(FontRenderSettings& settings) = 0;
};

#endif // TOOLKIT_UI_SETTINGS_H
