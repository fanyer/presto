/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_DISPLAY_DISPLAY_MODULE_H
#define MODULES_DISPLAY_DISPLAY_MODULE_H

#include "modules/hardcore/opera/module.h"

#include "modules/display/color.h"
#include "modules/display/fontcache.h"
#include "modules/display/styl_man.h"
#include "modules/display/webfont_manager.h"
#include "modules/dom/domeventtypes.h"

#ifdef DISPLAY_SUPPORT_LOCALIZED_FONT_NAMES
#include "modules/util/OpHashTable.h"
#endif // DISPLAY_SUPPORT_LOCALIZED_FONT_NAMES

#ifdef TEXTSHAPER_SUPPORT
#include "modules/display/textshaper.h"
#endif // TEXTSHAPER_SUPPORT

class MouseListenerClickInfo;
class CoreViewContainer;

#ifdef HAS_ATVEF_SUPPORT
class TvManager;
#endif // HAS_ATVEF_SUPPORT

class DisplayModule : public OperaModule
{
public:
	DisplayModule();

	void InitL(const OperaInitInfo& info);

	void Destroy();

#ifdef TEXTSHAPER_SUPPORT
	class TempBuffer *GetTextShaperBuf() { return m_textshaper_buf; }
#endif // TEXTSHAPER_SUPPORT

	class FontCache* font_cache;
	StyleManager* style_manager;
#ifdef DELAYED_STYLE_MANAGER_INIT
	BOOL creating_style_manager;
#endif // DELAYED_STYLE_MANAGER_INIT
	Style null_style;
	DOM_EventType current_mouse_event;
	MouseButton current_mouse_button;
	BOOL last_click_was_double;
	UINT8 last_mouse_down_count[3]; // Mouse button 1-3

	WebFontManager* webfont_manager;

	/** Default background color for webpages. */
	COLORREF		background_col;
	void			ReadColors();
	void			SetBackgroundColor(COLORREF col) { background_col = col; }
	COLORREF		GetBackgroundColor() const { return background_col; }

#ifdef TEXTSHAPER_SUPPORT
	TextShaper::JoiningState m_textshaper_state;
#endif // TEXTSHAPER_SUPPORT

#ifdef DISPLAY_CLICKINFO_SUPPORT
	MouseListenerClickInfo *mouse_click_info;
#endif // DISPLAY_CLICKINFO_SUPPORT

#ifdef DISPLAY_SUPPORT_LOCALIZED_FONT_NAMES
	/**
	   adds localized to the localized font name hash, and maps it to
	   western, so that when GetWesternFontName is passed localized it
	   will return western.  DisplayModule obtains ownership of
	   localized, but not western. western is assumed to exist as long
	   as opera is running.
	 */
	OP_STATUS AddLocalizedFontName(uni_char* localized, const uni_char* western);
	/**
	   returns the name mapped to localized, or 0 if no name has been
	   mapped.
	 */
	const uni_char* GetWesternFontName(const uni_char* localized);
#endif // DISPLAY_SUPPORT_LOCALIZED_FONT_NAMES
	CoreViewContainer *m_pending_release_capture;

#ifdef DELAYED_STYLE_MANAGER_INIT
	StyleManager* GetStyleManager();
#endif

#ifdef HAS_ATVEF_SUPPORT
	TvManager *m_tvManager;
#endif

#ifdef DRAG_SUPPORT
	OpPoint m_drag_point;
#endif // DRAG_SUPPORT

private:
#ifdef DISPLAY_SUPPORT_LOCALIZED_FONT_NAMES
	OpGenericStringHashTable m_localized_font_names;
#endif // DISPLAY_SUPPORT_LOCALIZED_FONT_NAMES

#ifdef TEXTSHAPER_SUPPORT
	class TempBuffer *m_textshaper_buf;
#endif // TEXTSHAPER_SUPPORT
};

#define colorManager (&g_opera->display_module)
#define g_font_cache g_opera->display_module.font_cache
#ifdef DELAYED_STYLE_MANAGER_INIT
#define styleManager g_opera->display_module.GetStyleManager()
#else
#define styleManager g_opera->display_module.style_manager
#endif
#define NullStyle (&(g_opera->display_module.null_style))
#define current_mouse_event g_opera->display_module.current_mouse_event
#define current_mouse_button g_opera->display_module.current_mouse_button
#define last_click_was_double g_opera->display_module.last_click_was_double
#define pending_release_capture g_opera->display_module.m_pending_release_capture

#ifdef TEXTSHAPER_SUPPORT
#define g_textshaper_state g_opera->display_module.m_textshaper_state
#endif // TEXTSHAPER_SUPPORT

#define g_webfont_manager g_opera->display_module.webfont_manager

#define g_display_module g_opera->display_module

#define DISPLAY_MODULE_REQUIRED

#endif // !MODULES_DISPLAY_DISPLAY_MODULE_H
