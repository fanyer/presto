/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/display/display_module.h"
#include "modules/display/fontcache.h"
#include "modules/display/VisDevListeners.h"
#ifdef HAS_ATVEF_SUPPORT
# include "modules/display/tvmanager.h"
#endif // HAS_ATVEF_SUPPORT
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"

DisplayModule::DisplayModule()
	: font_cache(NULL)
	, style_manager(NULL)
	  // current_mouse_event ?
	  // current_mouse_button ?
	  // last_click_was_double ?
	  // last_mouse_down_count[:] in body
#ifdef DELAYED_STYLE_MANAGER_INIT
	  , creating_style_manager(FALSE)
#endif
	, webfont_manager(NULL)
	  // background_col ?
#ifdef TEXTSHAPER_SUPPORT
	, m_textshaper_state(TextShaper::NOT_JOINING)
#endif // TEXTSHAPER_SUPPORT
#ifdef DISPLAY_CLICKINFO_SUPPORT
	, mouse_click_info(NULL)
#endif // DISPLAY_CLICKINFO_SUPPORT
	, m_pending_release_capture(NULL)
#ifdef HAS_ATVEF_SUPPORT
	, m_tvManager(NULL)
#endif
#ifdef DISPLAY_SUPPORT_LOCALIZED_FONT_NAMES
	, m_localized_font_names(FALSE)
#endif // DISPLAY_SUPPORT_LOCALIZED_FONT_NAMES
#ifdef TEXTSHAPER_SUPPORT
	, m_textshaper_buf(NULL)
#endif // TEXTSHAPER_SUPPORT
{
	last_mouse_down_count[0] = 0;
	last_mouse_down_count[1] = 0;
	last_mouse_down_count[2] = 0;
}

#ifdef DELAYED_STYLE_MANAGER_INIT
StyleManager* DisplayModule::GetStyleManager()
{
	if (!style_manager && !creating_style_manager)
	{
		creating_style_manager = TRUE;
		style_manager = StyleManager::Create();
		creating_style_manager = FALSE;
	}
	return style_manager;
}
#endif

void DisplayModule::InitL(const OperaInitInfo& info)
{
	font_cache = OP_NEW_L(FontCache, ());
#ifndef DELAYED_STYLE_MANAGER_INIT
	LEAVE_IF_NULL(style_manager = StyleManager::Create());
#endif

	current_mouse_event = ONMOUSEMOVE;
	current_mouse_button = MOUSE_BUTTON_1;
	last_click_was_double = FALSE;

#ifdef TEXTSHAPER_SUPPORT
	m_textshaper_buf = OP_NEW_L(TempBuffer, ());
//	m_textshaper_buf->SetExpansionPolicy(TempBuffer::TIGHT);
//	m_textshaper_buf->SetCachedLengthPolicy(TempBuffer::TRUSTED);
#endif // TEXTSHAPER_SUPPORT

#ifdef DISPLAY_CLICKINFO_SUPPORT
	mouse_click_info = OP_NEW_L(MouseListenerClickInfo, ());
#endif // DISPLAY_CLICKINFO_SUPPORT

	webfont_manager = OP_NEW_L(WebFontManager, ());

#ifdef HAS_ATVEF_SUPPORT
	m_tvManager = OP_NEW_L(TvManager, ());
#endif
}

void DisplayModule::Destroy()
{
	OP_DELETE(webfont_manager);
	webfont_manager = NULL;

	OP_DELETE(font_cache);
	font_cache = NULL;

	OP_DELETE(style_manager);
	style_manager = NULL;

#ifdef TEXTSHAPER_SUPPORT
	OP_DELETE(m_textshaper_buf);
	m_textshaper_buf = NULL;
#endif // TEXTSHAPER_SUPPORT

#ifdef DISPLAY_CLICKINFO_SUPPORT
	OP_DELETE(mouse_click_info);
#endif // DISPLAY_CLICKINFO_SUPPORT

#ifdef DISPLAY_SUPPORT_LOCALIZED_FONT_NAMES
	OpHashIterator* it = m_localized_font_names.GetIterator();
	if (it)
	{
		for (OP_STATUS s = it->First(); OpStatus::IsSuccess(s); s = it->Next())
		{
			uni_char* name = (uni_char*)it->GetKey();
			OP_DELETEA(name);
		}
		OP_DELETE(it);
	}
	else
	{
		OP_ASSERT(!"cannot clean up because of OOM");
	}
	m_localized_font_names.RemoveAll();
#endif // DISPLAY_SUPPORT_LOCALIZED_FONT_NAMES

#ifdef HAS_ATVEF_SUPPORT
	OP_DELETE(m_tvManager);
	m_tvManager = NULL;
#endif
}

void DisplayModule::ReadColors()
{
	background_col = g_pcfontscolors->GetColor(OP_SYSTEM_COLOR_DOCUMENT_BACKGROUND);
   	background_col = OP_RGB(OP_GET_R_VALUE(background_col), OP_GET_G_VALUE(background_col), OP_GET_B_VALUE(background_col));
}

#ifdef DISPLAY_SUPPORT_LOCALIZED_FONT_NAMES
OP_STATUS DisplayModule::AddLocalizedFontName(uni_char* localized, const uni_char* western)
{
	return m_localized_font_names.Add(localized, (void*)western);
}

const uni_char* DisplayModule::GetWesternFontName(const uni_char* localized)
{
	void* data;
	OP_STATUS s = m_localized_font_names.GetData(localized, &data);
	return OpStatus::IsSuccess(s) ? (const uni_char*)data : 0;
}
#endif // DISPLAY_SUPPORT_LOCALIZED_FONT_NAMES
