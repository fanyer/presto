/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 2003-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Morten Stenshorne
 */
#include "core/pch.h"

#include "platforms/unix/base/x11/x11_opsysteminfo.h"

#include "platforms/unix/base/common/unix_defaultstyle.h"

#include "modules/display/FontAtt.h"
#include "modules/pi/OpScreenInfo.h"
#include "modules/style/css.h"

#include "platforms/unix/base/x11/inpututils.h"
#include "platforms/unix/base/x11/x11_globals.h"
#include "platforms/unix/base/x11/x11_widget.h"

#include "platforms/quix/toolkits/ToolkitLibrary.h"
#include "platforms/quix/toolkits/ToolkitUiSettings.h"
#include "platforms/unix/product/x11quick/x11_global_desktop_application.h"

#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>

extern INT32 g_forced_dpi; // quick.cpp

INT32 X11OpSystemInfo::GetShiftKeyState()
{
	return InputUtils::GetOpModifierFlags();
}

UINT32 X11OpUiInfo::GetVerticalScrollbarWidth()
{
	return 16;
}

UINT32 X11OpUiInfo::GetHorizontalScrollbarHeight()
{
	return 16;
}

UINT32 X11OpUiInfo::GetSystemColor(OP_SYSTEM_COLOR color)
{
	if (!g_toolkit_library)
		return GetHardcodedColor(color);

	ToolkitUiSettings* settings = g_toolkit_library->GetUiSettings();

	switch (color)
	{
	case OP_SYSTEM_COLOR_BUTTON:
	case OP_SYSTEM_COLOR_UI_BUTTON_BACKGROUND:
	case OP_SYSTEM_COLOR_UI_HOTLIST_TREE_FONT:
		return MakeColor(settings->GetSystemColor(ToolkitUiSettings::COLOR_BUTTON));
	case OP_SYSTEM_COLOR_BUTTON_LIGHT:
		return MakeColor(settings->GetSystemColor(ToolkitUiSettings::COLOR_BUTTON_LIGHT));
	case OP_SYSTEM_COLOR_BUTTON_DARK:
		return MakeColor(settings->GetSystemColor(ToolkitUiSettings::COLOR_BUTTON_DARK));
	case OP_SYSTEM_COLOR_BUTTON_VERYDARK:
		return MakeColor(settings->GetSystemColor(ToolkitUiSettings::COLOR_BUTTON_VERYDARK));
	case OP_SYSTEM_COLOR_BUTTON_TEXT:
		return MakeColor(settings->GetSystemColor(ToolkitUiSettings::COLOR_BUTTON_TEXT));
	case OP_SYSTEM_COLOR_BACKGROUND:
	case OP_SYSTEM_COLOR_DOCUMENT_BACKGROUND:
		return MakeColor(settings->GetSystemColor(ToolkitUiSettings::COLOR_BACKGROUND));
	case OP_SYSTEM_COLOR_BACKGROUND_SELECTED:
		return MakeColor(settings->GetSystemColor(ToolkitUiSettings::COLOR_BACKGROUND_SELECTED));
	case OP_SYSTEM_COLOR_BACKGROUND_SELECTED_NOFOCUS:
		return MakeColor(settings->GetSystemColor(ToolkitUiSettings::COLOR_BACKGROUND_SELECTED_NOFOCUS));
	case OP_SYSTEM_COLOR_BACKGROUND_HIGHLIGHTED:
		return MakeColor(settings->GetSystemColor(ToolkitUiSettings::COLOR_BACKGROUND_HIGHLIGHTED));
	case OP_SYSTEM_COLOR_BACKGROUND_HIGHLIGHTED_NOFOCUS:
		return MakeColor(settings->GetSystemColor(ToolkitUiSettings::COLOR_BACKGROUND_HIGHLIGHTED_NOFOCUS));
	case OP_SYSTEM_COLOR_BACKGROUND_DISABLED:
		return MakeColor(settings->GetSystemColor(ToolkitUiSettings::COLOR_BACKGROUND_DISABLED));
	case OP_SYSTEM_COLOR_TEXT:
	case OP_SYSTEM_COLOR_TEXT_INPUT:
	case OP_SYSTEM_COLOR_ITEM_TEXT:
	case OP_SYSTEM_COLOR_DOCUMENT_NORMAL:
	case OP_SYSTEM_COLOR_DOCUMENT_HEADER1:
	case OP_SYSTEM_COLOR_DOCUMENT_HEADER2:
	case OP_SYSTEM_COLOR_DOCUMENT_HEADER3:
	case OP_SYSTEM_COLOR_DOCUMENT_HEADER4:
	case OP_SYSTEM_COLOR_DOCUMENT_HEADER5:
	case OP_SYSTEM_COLOR_DOCUMENT_HEADER6:
	case OP_SYSTEM_COLOR_DOCUMENT_PRE:
	case OP_SYSTEM_COLOR_HTML_COMPOSE_TEXT:
		return MakeColor(settings->GetSystemColor(ToolkitUiSettings::COLOR_TEXT));
	case OP_SYSTEM_COLOR_TEXT_SELECTED:
	case OP_SYSTEM_COLOR_ITEM_TEXT_SELECTED:
		return MakeColor(settings->GetSystemColor(ToolkitUiSettings::COLOR_TEXT_SELECTED));
	case OP_SYSTEM_COLOR_TEXT_SELECTED_NOFOCUS:
		return MakeColor(settings->GetSystemColor(ToolkitUiSettings::COLOR_TEXT_SELECTED_NOFOCUS));
	case OP_SYSTEM_COLOR_TEXT_HIGHLIGHTED:
		return MakeColor(settings->GetSystemColor(ToolkitUiSettings::COLOR_TEXT_HIGHLIGHTED));
	case OP_SYSTEM_COLOR_TEXT_HIGHLIGHTED_NOFOCUS:
		return MakeColor(settings->GetSystemColor(ToolkitUiSettings::COLOR_TEXT_HIGHLIGHTED_NOFOCUS));
	case OP_SYSTEM_COLOR_TEXT_DISABLED:
		return MakeColor(settings->GetSystemColor(ToolkitUiSettings::COLOR_TEXT_DISABLED));
	case OP_SYSTEM_COLOR_SCROLLBAR_BACKGROUND:
		return MakeColor(settings->GetSystemColor(ToolkitUiSettings::COLOR_SCROLLBAR_BACKGROUND));
	case OP_SYSTEM_COLOR_LINK:
		return MakeColor(settings->GetSystemColor(ToolkitUiSettings::COLOR_LINK));
	case OP_SYSTEM_COLOR_VISITED_LINK:
		return MakeColor(settings->GetSystemColor(ToolkitUiSettings::COLOR_VISITED_LINK));
	case OP_SYSTEM_COLOR_UI_FONT:
		return MakeColor(settings->GetSystemColor(ToolkitUiSettings::COLOR_UI_FONT));
	case OP_SYSTEM_COLOR_UI_DISABLED_FONT:
		return MakeColor(settings->GetSystemColor(ToolkitUiSettings::COLOR_UI_DISABLED_FONT));
	case OP_SYSTEM_COLOR_UI_BACKGROUND:
		return MakeColor(settings->GetSystemColor(ToolkitUiSettings::COLOR_UI_BACKGROUND));
	case OP_SYSTEM_COLOR_UI_BUTTON_HOVER:
	case OP_SYSTEM_COLOR_UI_PROGRESS:
	case OP_SYSTEM_COLOR_UI_WINBAR_ATTENTION:
		return MakeColor(settings->GetSystemColor(ToolkitUiSettings::COLOR_UI_BUTTON_HOVER));
	case OP_SYSTEM_COLOR_SKIN:
		return MakeColor(settings->GetSystemColor(ToolkitUiSettings::COLOR_SKIN));
	case OP_SYSTEM_COLOR_TOOLTIP_TEXT:
		return MakeColor(settings->GetSystemColor(ToolkitUiSettings::COLOR_TOOLTIP_TEXT));
	case OP_SYSTEM_COLOR_TOOLTIP_BACKGROUND:
		return MakeColor(settings->GetSystemColor(ToolkitUiSettings::COLOR_TOOLTIP_BACKGROUND));
	case OP_SYSTEM_COLOR_UI_MENU:
		return MakeColor(settings->GetSystemColor(ToolkitUiSettings::COLOR_UI_MENU));
	case OP_SYSTEM_COLOR_WORKSPACE:
		return MakeColor(settings->GetSystemColor(ToolkitUiSettings::COLOR_WORKSPACE));
	default:
#ifdef DEBUG
		fprintf(stderr, "X11OpSystemInfo::GetSystemColor(): Unknown color code %d. Using red.\n", color);
		return OP_RGB(0xff, 0, 0);
#else
		return OP_RGB(0xff, 0xff, 0xff);
#endif
	}
}

UINT32 X11OpUiInfo::GetHardcodedColor(OP_SYSTEM_COLOR color)
{
	switch (color)
	{
		case OP_SYSTEM_COLOR_BUTTON: return OP_RGB(0xc0, 0xc0, 0xc0); 
		case OP_SYSTEM_COLOR_BUTTON_LIGHT: return OP_RGB(0xdf, 0xdf, 0xdf); 
		case OP_SYSTEM_COLOR_BUTTON_DARK: return OP_RGB(0x80, 0x80, 0x80); 
		case OP_SYSTEM_COLOR_BUTTON_VERYDARK: return OP_RGB(0, 0, 0); 
		case OP_SYSTEM_COLOR_BUTTON_TEXT: return OP_RGB(0, 0, 0); 
		case OP_SYSTEM_COLOR_BACKGROUND: return OP_RGB(0xff, 0xff, 0xff); 
		case OP_SYSTEM_COLOR_BACKGROUND_SELECTED: return OP_RGB(0, 0, 0x80); 
		case OP_SYSTEM_COLOR_BACKGROUND_SELECTED_NOFOCUS: return OP_RGB(0xc0, 0xc0, 0xc0); 
		case OP_SYSTEM_COLOR_BACKGROUND_HIGHLIGHTED: return OP_RGB(0xff, 0xc0, 0xc0); 
		case OP_SYSTEM_COLOR_BACKGROUND_HIGHLIGHTED_NOFOCUS: return OP_RGB(0xff, 0xc0, 0xc0); 
		case OP_SYSTEM_COLOR_BACKGROUND_DISABLED: return OP_RGB(0xc0, 0xc0, 0xc0); 
		case OP_SYSTEM_COLOR_TEXT: return OP_RGB(0, 0, 0); 
		case OP_SYSTEM_COLOR_TEXT_INPUT: return OP_RGB(0, 0, 0); 
		case OP_SYSTEM_COLOR_TEXT_SELECTED: return OP_RGB(0xff, 0xff, 0xff); 
		case OP_SYSTEM_COLOR_TEXT_SELECTED_NOFOCUS: return OP_RGB(0, 0, 0); 
		case OP_SYSTEM_COLOR_TEXT_HIGHLIGHTED: return OP_RGB(0xff, 0xff, 0xff); 
		case OP_SYSTEM_COLOR_TEXT_HIGHLIGHTED_NOFOCUS: return OP_RGB(0xff, 0xff, 0xff); 
		case OP_SYSTEM_COLOR_TEXT_DISABLED: return OP_RGB(0x80, 0x80, 0x80); 
		case OP_SYSTEM_COLOR_ITEM_TEXT: return OP_RGB(0, 0, 0); 
		case OP_SYSTEM_COLOR_ITEM_TEXT_SELECTED: return OP_RGB(0xff, 0xff, 0xff); 
		case OP_SYSTEM_COLOR_SCROLLBAR_BACKGROUND: return OP_RGB(0xa0, 0xa0, 0xa0); 
		case OP_SYSTEM_COLOR_DOCUMENT_NORMAL: return OP_RGB(0, 0, 0); 
		case OP_SYSTEM_COLOR_DOCUMENT_HEADER1: return OP_RGB(0, 0, 0); 
		case OP_SYSTEM_COLOR_DOCUMENT_HEADER2: return OP_RGB(0, 0, 0); 
		case OP_SYSTEM_COLOR_DOCUMENT_HEADER3: return OP_RGB(0, 0, 0); 
		case OP_SYSTEM_COLOR_DOCUMENT_HEADER4: return OP_RGB(0, 0, 0); 
		case OP_SYSTEM_COLOR_DOCUMENT_HEADER5: return OP_RGB(0, 0, 0); 
		case OP_SYSTEM_COLOR_DOCUMENT_HEADER6: return OP_RGB(0, 0, 0); 
		case OP_SYSTEM_COLOR_DOCUMENT_BACKGROUND: return OP_RGB(0xff, 0xff, 0xff); 
		case OP_SYSTEM_COLOR_DOCUMENT_PRE: return OP_RGB(0, 0, 0); 
		case OP_SYSTEM_COLOR_LINK: return OP_RGB(0, 0, 0xcc); 
		case OP_SYSTEM_COLOR_VISITED_LINK: return OP_RGB(0x80, 0, 0x80); 
		case OP_SYSTEM_COLOR_UI_FONT: return OP_RGB(0, 0, 0); 
		case OP_SYSTEM_COLOR_UI_DISABLED_FONT: return OP_RGB(0x80, 0x80, 0x80); 
		case OP_SYSTEM_COLOR_UI_BACKGROUND: return OP_RGB(0xc0, 0xc0, 0xc0); 
		case OP_SYSTEM_COLOR_UI_BUTTON_BACKGROUND: return OP_RGB(0xaa, 0xaa, 0xaa); 
		case OP_SYSTEM_COLOR_UI_PROGRESS: return OP_RGB(0, 0, 0x80); 
		case OP_SYSTEM_COLOR_UI_HOTLIST_TREE_FONT: return OP_RGB(0, 0, 0); 
		case OP_SYSTEM_COLOR_UI_WINBAR_ATTENTION: return OP_RGB(0, 0, 0); 
		case OP_SYSTEM_COLOR_UI_BUTTON_HOVER: return OP_RGB(0, 0, 0); 
		case OP_SYSTEM_COLOR_SKIN: return OP_RGB(0xa0, 0xa0, 0xa0); 
		case OP_SYSTEM_COLOR_TOOLTIP_TEXT: return OP_RGB(0xa0, 0xa0, 0xa0); 
		case OP_SYSTEM_COLOR_HTML_COMPOSE_TEXT: return OP_RGB(0xa0, 0xa0, 0xa0); 
		case OP_SYSTEM_COLOR_WORKSPACE: return OP_RGB(0xa0, 0xa0, 0xa0);
		default: 
#ifdef DEBUG 
			printf("X11OpSystemInfo::GetHardcodedColor(): Unknown color code %d. Using red.\n", color);
			return OP_RGB(0xff, 0, 0); 
#else 
			return OP_RGB(0xff, 0xff, 0xff); 
#endif
	}
}

void X11OpUiInfo::GetSystemFont(OP_SYSTEM_FONT font, FontAtt &outfont)
{
	enum FontHeightPx {
		HEIGHT_UI=12,
		HEIGHT_FORM=12,
		HEIGHT_NORMAL=16,
		HEIGHT_MAIL_DISPLAY=13 // Changed from 16 (bug #203163)
	};

	GetVerticalDpi();

	ToolkitUiSettings::FontDetails font_details;
	if (g_toolkit_library)
		g_toolkit_library->GetUiSettings()->GetDefaultFont(font_details);

	int height = HEIGHT_NORMAL;
	int weight = 4;

	switch (font)
	{
		case OP_SYSTEM_FONT_DOCUMENT_NORMAL:
			break;
		case OP_SYSTEM_FONT_DOCUMENT_HEADER1:
			height = height * 2;
			weight = 7;
			break;
		case OP_SYSTEM_FONT_UI_HEADER:
		case OP_SYSTEM_FONT_DOCUMENT_HEADER2:
			height = height * 3 / 2;
			weight = 7;
			break;
		case OP_SYSTEM_FONT_DOCUMENT_HEADER3:
			height = height * 7 / 6;
			weight = 7;
			break;
		case OP_SYSTEM_FONT_DOCUMENT_HEADER4:
			weight = 7;
			break;
		case OP_SYSTEM_FONT_DOCUMENT_HEADER5:
			height = height * 5 / 6;
			weight = 7;
			break;
		case OP_SYSTEM_FONT_DOCUMENT_HEADER6:
			height = height * 3 / 4;
			weight = 7;
			break;
		case OP_SYSTEM_FONT_DOCUMENT_PRE:
			break;
		case OP_SYSTEM_FONT_FORM_TEXT_INPUT:
		case OP_SYSTEM_FONT_FORM_BUTTON:
		case OP_SYSTEM_FONT_FORM_TEXT:
		case OP_SYSTEM_FONT_EMAIL_COMPOSE:
		case OP_SYSTEM_FONT_HTML_COMPOSE:
			height = HEIGHT_FORM;
			break;
		case OP_SYSTEM_FONT_EMAIL_DISPLAY:
			height = HEIGHT_MAIL_DISPLAY;
			break;
		case OP_SYSTEM_FONT_UI_TOOLBAR:
		case OP_SYSTEM_FONT_UI_DIALOG:
		case OP_SYSTEM_FONT_UI_PANEL:
		case OP_SYSTEM_FONT_UI_TOOLTIP:
		case OP_SYSTEM_FONT_UI:
		case OP_SYSTEM_FONT_UI_DISABLED:
		case OP_SYSTEM_FONT_UI_HOTLIST_TREE:
		case OP_SYSTEM_FONT_UI_MENU:
			height = ConvertToPixelSize(font_details.size);
			break;
	}

	outfont.SetSize(height);
	outfont.SetWeight(weight);
	OpFontManager *fontManager = styleManager->GetFontManager();

	switch (font)
	{
		case OP_SYSTEM_FONT_DOCUMENT_PRE:
		case OP_SYSTEM_FONT_FORM_TEXT:
		case OP_SYSTEM_FONT_EMAIL_COMPOSE:
		case OP_SYSTEM_FONT_HTML_COMPOSE:
		case OP_SYSTEM_FONT_EMAIL_DISPLAY:
			outfont.SetFaceName(fontManager->GetGenericFontName(GENERIC_FONT_MONOSPACE));
			break;
		case OP_SYSTEM_FONT_FORM_BUTTON:
		case OP_SYSTEM_FONT_FORM_TEXT_INPUT:
		case OP_SYSTEM_FONT_UI_MENU:
		case OP_SYSTEM_FONT_UI_TOOLBAR:
		case OP_SYSTEM_FONT_UI_DIALOG:
		case OP_SYSTEM_FONT_UI_PANEL:
		case OP_SYSTEM_FONT_UI_HEADER:
		case OP_SYSTEM_FONT_UI_TOOLTIP:
		{
			OpString font_name;
			font_name.Set(font_details.family);
			int font_number = font_name.HasContent() ? styleManager->GetFontNumber(font_name.CStr()) : -1;
			if (font_number >= 0)
				outfont.SetFontNumber(font_number);
			else
			{
				switch (font_details.type)
				{
					case ToolkitUiSettings::SANSSERIF:
						outfont.SetFaceName(fontManager->GetGenericFontName(GENERIC_FONT_SANSSERIF));
						break;
					case ToolkitUiSettings::SERIF:
						outfont.SetFaceName(fontManager->GetGenericFontName(GENERIC_FONT_SERIF));
						break;
					case ToolkitUiSettings::MONOSPACE:
						outfont.SetFaceName(fontManager->GetGenericFontName(GENERIC_FONT_MONOSPACE));
						break;
				}
			}
			break;
		}
		default:
			outfont.SetFaceName(fontManager->GetGenericFontName(GENERIC_FONT_SERIF));
	}
}

void X11OpUiInfo::GetVerticalDpi()
{
	if (m_vertical_dpi)
		return;

	g_op_screen_info->GetDPI(0, &m_vertical_dpi, NULL, NULL);

	// The forced dpi value gets read from prefs on startup, but we
	// cannot use the prefs function because we are here before the
	// unix-prefs-object has been initialized.
	if( g_forced_dpi > 10 )
	{
		m_vertical_dpi = g_forced_dpi;
	}
}

COLORREF X11OpUiInfo::GetUICSSColor(int css_color_value)
{
	UINT32 col = 0xffffff;

	switch (css_color_value)
	{
	case CSS_VALUE_ActiveBorder: col = 0x000000; break;
	case CSS_VALUE_ActiveCaption: col = 0x000000; break;
	case CSS_VALUE_AppWorkspace: col = 0x777777; break;
	case CSS_VALUE_Background: col = 0xffffff; break;
	case CSS_VALUE_ButtonHighlight: col = 0xdddddd; break;
	case CSS_VALUE_ButtonShadow: col = 0x444444; break;
	case CSS_VALUE_ButtonText: col = 0x000000; break;
	case CSS_VALUE_CaptionText: col = 0x000000; break;
	case CSS_VALUE_GrayText: col = 0xaaaaaa; break;
	case CSS_VALUE_Highlight: col = 0x000099; break;
	case CSS_VALUE_HighlightText: col = 0xffffff; break;
	case CSS_VALUE_InactiveBorder: col = 0xcccccc; break;
	case CSS_VALUE_InactiveCaption: col = 0x444444; break;
	case CSS_VALUE_InactiveCaptionText: col = 0x888888; break;
	case CSS_VALUE_InfoBackground: col = 0xffffe1; break;
	case CSS_VALUE_InfoText: col = 0x000000; break;
	case CSS_VALUE_Menu: col = 0x888888; break;
	case CSS_VALUE_MenuText: col = 0x000000; break;
	case CSS_VALUE_Scrollbar: col = 0x999999; break;
	case CSS_VALUE_ThreeDDarkShadow: col = 0x444444; break;
	case CSS_VALUE_ThreeDFace: col = 0xaaaaaa; break;
	case CSS_VALUE_ThreeDHighlight: col = 0xcccccc; break;
	case CSS_VALUE_ThreeDLightShadow: col = 0x666666; break;
	case CSS_VALUE_ThreeDShadow: col = 0x333333; break;
	case CSS_VALUE_Window: col = 0xffffff; break;
	case CSS_VALUE_WindowFrame: col = 0x555555; break;
	case CSS_VALUE_WindowText: col = 0x000000; break;
	default:
#ifdef DEBUG
		printf("X11OpSystemInfo::GetUICSSColor(): Unknown color code %d. Using magenta\n", css_color_value);
		col = 0xff00ff;
#else
		col = 0xffffff;
#endif
		break;
	}

	return OP_RGB((col >> 16) & 0xff, (col >> 8) & 0xff, col & 0xff);
}

BOOL X11OpUiInfo::GetUICSSFont(int css_font_value, FontAtt &font)
{
	// spec says:
	// If no font with the indicated characteristics exists on a given
	// platform, the user agent should either intelligently substitute 
	// (e.g., a smaller version of the "caption" font might be used for
	// the "smallcaption" font), or substitute a user agent default font.
	// http://www.w3.org/TR/css3-fonts/#font
	//
	// But no *nix browser nor Opera on other platforms substitutes
	// "intelligently" atm and we have no good estimate, what is nice size for
	// small_caption. [pobara 2010-05-18]
	OP_SYSTEM_FONT sys_font;
	switch(css_font_value)
	{
	case CSS_VALUE_menu:		sys_font = OP_SYSTEM_FONT_UI_MENU; break;
	case CSS_VALUE_message_box:	sys_font = OP_SYSTEM_FONT_UI_DIALOG; break;
	case CSS_VALUE_caption:
	case CSS_VALUE_icon:
	case CSS_VALUE_small_caption:
	case CSS_VALUE_status_bar:	sys_font = OP_SYSTEM_FONT_UI; break;
	default:
#ifdef DEBUG
		printf("X11OpUiInfo::GetUICSSFont(): Unknown font code %d.\n", css_font_value);
#endif
		return FALSE;
	}
	GetSystemFont(sys_font, font);
	return TRUE;
}

BOOL X11OpUiInfo::IsMouseRightHanded()
{
	return TRUE;
}

BOOL X11OpUiInfo::IsFullKeyboardAccessActive()
{
	return TRUE;
}

UINT32 X11OpUiInfo::GetCaretWidth()
{
	return 1;
}

BOOL X11OpUiInfo::DefaultButtonOnRight()
{
	return g_toolkit_library->GetUiSettings()->DefaultButtonOnRight();
}

BOOL X11OpSystemInfo::HasSystemTray()
{
	return static_cast<X11DesktopGlobalApplication*>(
			g_desktop_global_application)->HasSystemTrayIcon();
}

