/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/pi/ui/MacOpUiInfo.h"
#include "platforms/mac/pi/MacOpSystemInfo.h"

#include "platforms/mac/model/MacFontIntern.h"
#include "platforms/mac/util/CTextConverter.h"
#include "platforms/mac/util/MachOCompatibility.h"
#include "modules/style/src/css_values.h"

#include "adjunct/quick/Application.h"
#include "adjunct/desktop_util/string/stringutils.h"

#if defined(_QUICK_UI_FONT_SUPPORT_)
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"
#endif // _QUICK_UI_FONT_SUPPORT_

#define RGB2COLORREF(color) (OP_RGB((color.red & 0xFF00) >> 8, (color.green & 0xFF00) >> 8, (color.blue & 0xFF00) >> 8))

Boolean g_preferences_dirty = true;

extern const uni_char* GetSystemAcceptLanguage();

UINT32 GetSystemHilightColor(BOOL active)
{
	CGColorRef hiliteColor;
	if (noErr == HIThemeBrushCreateCGColor(active ? kThemeBrushPrimaryHighlightColor : kThemeBrushSecondaryHighlightColor, &hiliteColor))
	{
		const CGFloat * comps = CGColorGetComponents(hiliteColor);
		UINT32 color = OP_RGB(comps[0]*255, comps[1]*255, comps[2]*255);
		CGColorRelease(hiliteColor);
		return color;
	}
	return OP_RGB(0xD0, 0xD0, 0xD0);
}

OP_STATUS OpUiInfo::Create(OpUiInfo** new_opuiinfo)
{
	*new_opuiinfo = new MacOpUiInfo;
	return *new_opuiinfo ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}


MacOpUiInfo::MacOpUiInfo()
	: m_use_custom_highlight(0),
	  m_full_keyboard_access(2)
{
}

UINT32 MacOpUiInfo::GetVerticalScrollbarWidth()
{
	return 15;
}

UINT32 MacOpUiInfo::GetHorizontalScrollbarHeight()
{
	return 15;
}

UINT32 MacOpUiInfo::GetSystemColor(OP_SYSTEM_COLOR color)
{
	CGColorRef hiliteColor;
	switch (color)
	{
		case OP_SYSTEM_COLOR_BUTTON:
			return OP_RGB(0xDD, 0xDD, 0xDD);
		case OP_SYSTEM_COLOR_BUTTON_LIGHT:
			return OP_RGB(0xFF, 0xFF, 0xFF);
		case OP_SYSTEM_COLOR_BUTTON_DARK:
			return OP_RGB(0xAA, 0xAA, 0xAA);
		case OP_SYSTEM_COLOR_BUTTON_VERYDARK:
			return OP_RGB(0x77, 0x77, 0x77);
		case OP_SYSTEM_COLOR_BUTTON_TEXT:
			return OP_RGB(0x00, 0x00, 0x00);
		case OP_SYSTEM_COLOR_BACKGROUND:
			return OP_RGB(0xFF, 0xFF, 0xFF);
		case OP_SYSTEM_COLOR_BACKGROUND_SELECTED:
			if (m_use_custom_highlight)
			{
				return m_custom_highlight;
			}
			return GetSystemHilightColor(TRUE);
		case OP_SYSTEM_COLOR_BACKGROUND_SELECTED_NOFOCUS:
			if (m_use_custom_highlight)
			{
				return m_custom_highlight;
			}
			return GetSystemHilightColor(FALSE);
		case OP_SYSTEM_COLOR_BACKGROUND_DISABLED:
			return OP_RGB(0xFF, 0xFF, 0xFF);
		case OP_SYSTEM_COLOR_TEXT_SELECTED:
			if (noErr == HIThemeBrushCreateCGColor(kThemeBrushPrimaryHighlightColor, &hiliteColor))
			{
				UINT32 color = OP_RGB(0x00, 0x00, 0x00);
				const CGFloat * comps = CGColorGetComponents(hiliteColor);
				if (comps[0]*255 + comps[1]*255 + comps[2]*255 < 20)
				{
					color = OP_RGB(0xFF, 0xFF, 0xFF);
				}
				CGColorRelease(hiliteColor);
				return color;
			}
			return OP_RGB(0x00, 0x00, 0x00);
		case OP_SYSTEM_COLOR_TEXT:
		case OP_SYSTEM_COLOR_TEXT_INPUT:
		case OP_SYSTEM_COLOR_TEXT_SELECTED_NOFOCUS:
			return OP_RGB(0x00, 0x00, 0x00);

#ifdef PI_CAP_HIGHLIGHT_COLORS
        case OP_SYSTEM_COLOR_TEXT_HIGHLIGHTED:
			return OP_RGB(0xFF, 0xFF, 0xFF);

        case OP_SYSTEM_COLOR_TEXT_HIGHLIGHTED_NOFOCUS:
			return OP_RGB(0x00, 0x00, 0x00);

        case OP_SYSTEM_COLOR_BACKGROUND_HIGHLIGHTED:
			return OP_RGB(0x23, 0xDC, 0x23);

        case OP_SYSTEM_COLOR_BACKGROUND_HIGHLIGHTED_NOFOCUS:
			return OP_RGB(0x00, 0xFF, 0xFF);
#endif //PI_CAP_HIGHLIGHT_COLORS

		case OP_SYSTEM_COLOR_TEXT_DISABLED:
			return OP_RGB(0x7F, 0x7F, 0x7F);

		case OP_SYSTEM_COLOR_DOCUMENT_NORMAL:
		case OP_SYSTEM_COLOR_DOCUMENT_HEADER1:
		case OP_SYSTEM_COLOR_DOCUMENT_HEADER2:
		case OP_SYSTEM_COLOR_DOCUMENT_HEADER3:
		case OP_SYSTEM_COLOR_DOCUMENT_HEADER4:
		case OP_SYSTEM_COLOR_DOCUMENT_HEADER5:
		case OP_SYSTEM_COLOR_DOCUMENT_HEADER6:
			return OP_RGB(0x00, 0x00, 0x00);

		case OP_SYSTEM_COLOR_DOCUMENT_BACKGROUND:
			return OP_RGB(0xFF, 0xFF, 0xFF);

		case OP_SYSTEM_COLOR_DOCUMENT_PRE:
			return OP_RGB(0x00, 0x00, 0x00);

		case OP_SYSTEM_COLOR_SCROLLBAR_BACKGROUND:
			return OP_RGB(0xAA, 0xAA, 0xAA);

		case OP_SYSTEM_COLOR_UI_FONT:
			return OP_RGB(0x00, 0x00, 0x00);

		case OP_SYSTEM_COLOR_UI_DISABLED_FONT:
			return OP_RGB(0x7F, 0x7F, 0x7F);

		case OP_SYSTEM_COLOR_UI_BACKGROUND:
		case OP_SYSTEM_COLOR_UI_BUTTON_BACKGROUND:
			return OP_RGB(0xCC, 0xCC, 0xCC);

		case OP_SYSTEM_COLOR_UI_PROGRESS:
			return OP_RGB(0x7F, 0x7F, 0xFF);
		case OP_SYSTEM_COLOR_UI_HOTLIST_TREE_FONT:
			return OP_RGB(0x00, 0x00, 0x00);
		case OP_SYSTEM_COLOR_UI_WINBAR_ATTENTION:
			return OP_RGB(0x00, 0x00, 0x00);
		case OP_SYSTEM_COLOR_UI_BUTTON_HOVER:
			return OP_RGB(0x00, 0x00, 0x00);
		case OP_SYSTEM_COLOR_SKIN:
			return OP_RGB(0xCC, 0xCC, 0xCC);
		case OP_SYSTEM_COLOR_LINK:
			return OP_RGB(0x00, 0x00, 0xCC);
		case OP_SYSTEM_COLOR_VISITED_LINK:
			return OP_RGB(0x80, 0x00, 0x80);
		case OP_SYSTEM_COLOR_WORKSPACE:
			return OP_RGB(0x80, 0x80, 0x80);
	}
	return OP_RGB(0xCC, 0xCC, 0xCC);
}

void MacOpUiInfo::GetSystemFont(OP_SYSTEM_FONT font, FontAtt &outfont)
{
	static short height12 = 16;

	outfont.SetItalic(FALSE);
	outfont.SetUnderline(FALSE);
	outfont.SetStrikeOut(FALSE);

	OpFontManager *fontManager = styleManager->GetFontManager();

	switch (font)
	{
		case OP_SYSTEM_FONT_DOCUMENT_NORMAL:
			outfont.SetFaceName(fontManager->GetGenericFontName(GENERIC_FONT_SERIF));
			outfont.SetSize(height12);
			outfont.SetWeight(4);
			break;

		case OP_SYSTEM_FONT_DOCUMENT_HEADER1:
			outfont.SetFaceName(fontManager->GetGenericFontName(GENERIC_FONT_SERIF));
			outfont.SetSize(height12 + 12);
			outfont.SetWeight(7);
			break;

		case OP_SYSTEM_FONT_DOCUMENT_HEADER2:
			outfont.SetFaceName(fontManager->GetGenericFontName(GENERIC_FONT_SERIF));
			outfont.SetSize(height12 + 6);
			outfont.SetWeight(7);
			break;

		case OP_SYSTEM_FONT_DOCUMENT_HEADER3:
			outfont.SetFaceName(fontManager->GetGenericFontName(GENERIC_FONT_SERIF));
			outfont.SetSize(height12 + 2);
			outfont.SetWeight(7);
			break;

		case OP_SYSTEM_FONT_DOCUMENT_HEADER4:
			outfont.SetFaceName(fontManager->GetGenericFontName(GENERIC_FONT_SERIF));
			outfont.SetSize(height12);
			outfont.SetWeight(7);
			break;

		case OP_SYSTEM_FONT_DOCUMENT_HEADER5:
			outfont.SetFaceName(fontManager->GetGenericFontName(GENERIC_FONT_SERIF));
			outfont.SetSize(height12 - 2);
			outfont.SetWeight(7);
			break;

		case OP_SYSTEM_FONT_DOCUMENT_HEADER6:
			outfont.SetFaceName(fontManager->GetGenericFontName(GENERIC_FONT_SERIF));
			outfont.SetSize(height12 - 4);
			outfont.SetWeight(7);
			break;

		case OP_SYSTEM_FONT_DOCUMENT_PRE:
			outfont.SetFaceName(fontManager->GetGenericFontName(GENERIC_FONT_MONOSPACE));
			outfont.SetSize(height12);
			outfont.SetWeight(4);
			break;

		case OP_SYSTEM_FONT_EMAIL_COMPOSE:
		case OP_SYSTEM_FONT_EMAIL_DISPLAY:
			outfont.SetFaceName(fontManager->GetGenericFontName(GENERIC_FONT_MONOSPACE));
			outfont.SetSize(12);
			outfont.SetWeight(4);
			break;

		case OP_SYSTEM_FONT_HTML_COMPOSE:
			outfont.SetFaceName(fontManager->GetGenericFontName(GENERIC_FONT_SANSSERIF));
			outfont.SetSize(12);
			outfont.SetWeight(4);
			break;

		case OP_SYSTEM_FONT_FORM_TEXT:
		case OP_SYSTEM_FONT_FORM_TEXT_INPUT:
			outfont.SetFaceName(FONT_LUCIDA_GRANDE);
			outfont.SetSize(11);
			outfont.SetWeight(4);
			break;

#if defined(_QUICK_UI_FONT_SUPPORT_)
		case OP_SYSTEM_FONT_UI_MENU:
			GetUICSSFont(CSS_VALUE_message_box, outfont);
			break;

		case OP_SYSTEM_FONT_FORM_BUTTON:
//		case STYLE_EX_FORM_SELECT:
			GetUICSSFont(CSS_VALUE_small_caption, outfont);
			break;

		case OP_SYSTEM_FONT_UI_DIALOG:
			GetUICSSFont(CSS_VALUE_caption, outfont);
			break;
		case OP_SYSTEM_FONT_UI_TOOLBAR:
			GetUICSSFont(CSS_VALUE_message_box, outfont);
			break;
		case OP_SYSTEM_FONT_UI_PANEL:
			GetUICSSFont(CSS_VALUE_message_box, outfont);
			break;
#endif
		case OP_SYSTEM_FONT_UI:
		case OP_SYSTEM_FONT_UI_DISABLED:
		case OP_SYSTEM_FONT_UI_HOTLIST_TREE:
		case OP_SYSTEM_FONT_UI_TOOLTIP:
			outfont.SetFaceName(fontManager->GetGenericFontName(GENERIC_FONT_SANSSERIF));
			outfont.SetFaceName(FONT_LUCIDA_GRANDE);
			outfont.SetSize(10);
			outfont.SetWeight(4);
			break;

		case OP_SYSTEM_FONT_UI_HEADER:
			outfont.SetFaceName(FONT_HELVETICA);
			outfont.SetSize(24);
			outfont.SetWeight(7);
			break;

		default:
			return;
	}
}

#if defined(_QUICK_UI_FONT_SUPPORT_)
void MacOpUiInfo::GetFont(OP_SYSTEM_FONT font, FontAtt &retval, BOOL use_system_default)
{
	// We only use system default font for the select font dialog (SelectFontDialog::GetDefaultFont)
	if (use_system_default)
	{
		switch (font)
		{
			case OP_SYSTEM_FONT_EMAIL_COMPOSE:
			case OP_SYSTEM_FONT_HTML_COMPOSE:
			case OP_SYSTEM_FONT_EMAIL_DISPLAY:
				{
					Style *s = styleManager->GetStyleEx(STYLE_EX_FORM_TEXTAREA);
					PresentationAttr p = s->GetPresentationAttr();
					retval = *(p.GetPresentationFont(StringUtils::GetDefaultWritingSystem()).Font);
				}
				break;
			case OP_SYSTEM_FONT_UI_MENU:
				GetUICSSFont(CSS_VALUE_menu, retval);
				break;
			case OP_SYSTEM_FONT_UI_HEADER:
			{
				retval.SetFaceName(FONT_HELVETICA);
				retval.SetSize(24);
				retval.SetWeight(7);
				break;
			}
			case OP_SYSTEM_FONT_UI_TOOLBAR:
				{
					retval.SetFaceName(FONT_LUCIDA_GRANDE);
					retval.SetSize(11);
					retval.SetWeight(4);
					retval.SetUnderline(FALSE);
					retval.SetItalic(FALSE);
					retval.SetStrikeOut(FALSE);
				}
				break;
			case OP_SYSTEM_FONT_UI:
			case OP_SYSTEM_FONT_UI_DISABLED:
			case OP_SYSTEM_FONT_UI_HOTLIST_TREE:
			case OP_SYSTEM_FONT_UI_PANEL:
			case OP_SYSTEM_FONT_UI_TOOLTIP:
				retval.SetFaceName(FONT_LUCIDA_GRANDE);
				retval.SetSize(10);
				retval.SetWeight(4);
				break;
			case OP_SYSTEM_FONT_UI_DIALOG:
				GetUICSSFont(CSS_VALUE_caption, retval);
				break;
			default:
				GetUICSSFont(CSS_VALUE_message_box, retval);
				break;
		}
		return;
	}
	else
	{
		switch (font)
		{
			case OP_SYSTEM_FONT_UI_ADDRESSFIELD:
			case OP_SYSTEM_FONT_UI_TOOLBAR:
				retval.SetFaceName(FONT_LUCIDA_GRANDE);
				retval.SetSize(11);
				retval.SetWeight(4);
				break;
			case OP_SYSTEM_FONT_UI_TREEVIEW_HEADER:
			case OP_SYSTEM_FONT_UI_MENU:
			case OP_SYSTEM_FONT_UI_DIALOG:
				retval.SetFaceName(FONT_LUCIDA_GRANDE);
				retval.SetSize(13);
				retval.SetWeight(4);
			break;
			case OP_SYSTEM_FONT_UI_TREEVIEW:
			case OP_SYSTEM_FONT_UI_PANEL:
				retval.SetFaceName(FONT_LUCIDA_GRANDE);
				retval.SetSize(12);
				retval.SetWeight(4);
				break;
			case OP_SYSTEM_FONT_UI_HEADER:
			{
				retval.SetFaceName(FONT_HELVETICA);
				retval.SetSize(24);
				retval.SetWeight(7);
				break;
			}
			default:
				g_pcfontscolors->GetFont(font, retval);
				break;
		}
	}
}
#endif

COLORREF MacOpUiInfo::GetUICSSColor(int css_color_value)
{
	switch (css_color_value)
	{
		case CSS_VALUE_ActiveBorder:			return OP_RGB(0xCC, 0xCC, 0xCC);	// Border color on window, active
		case CSS_VALUE_ActiveCaption:			return OP_RGB(0xCC, 0xCC, 0xCC);	// Titlebar color on window, active
		case CSS_VALUE_AppWorkspace:			return OP_RGB(0x63, 0x63, 0x9C);	// Background color of MDI parent ?
		case CSS_VALUE_Background:				return OP_RGB(0x63, 0x63, 0x9C);	// Desktop background color
		case CSS_VALUE_ButtonHighlight:			return OP_RGB(0xFF, 0xFF, 0xFF);	// 3D hilight on pushbuttons
		case CSS_VALUE_ButtonShadow:			return OP_RGB(0x88, 0x88, 0x88);	// 3D shadow on pushbuttons
		case CSS_VALUE_ButtonText:				return OP_RGB(0x00, 0x00, 0x00);
		case CSS_VALUE_CaptionText:				return OP_RGB(0x00, 0x00, 0x00);
		case CSS_VALUE_GrayText:				return OP_RGB(0x7F, 0x7F, 0x7F);
		case CSS_VALUE_Highlight:				return m_use_custom_highlight ? m_custom_highlight : GetSystemHilightColor(TRUE);
		case CSS_VALUE_HighlightText:			return OP_RGB(0x00, 0x00, 0x00);
		case CSS_VALUE_InactiveBorder:			return OP_RGB(0xDD, 0xDD, 0xDD);
		case CSS_VALUE_InactiveCaption:			return OP_RGB(0xDD, 0xDD, 0xDD);
		case CSS_VALUE_InactiveCaptionText:		return OP_RGB(0x77, 0x77, 0x77);
		case CSS_VALUE_InfoBackground:			return OP_RGB(0xFF, 0xFF, 0xCA);
		case CSS_VALUE_InfoText:				return OP_RGB(0x00, 0x00, 0x00);
		case CSS_VALUE_Menu:					return OP_RGB(0xDD, 0xDD, 0xDD);
		case CSS_VALUE_MenuText:				return OP_RGB(0x00, 0x00, 0x00);
		case CSS_VALUE_Scrollbar:				return OP_RGB(0xAA, 0xAA, 0xAA);
		case CSS_VALUE_ThreeDDarkShadow:		return OP_RGB(0x77, 0x77, 0x77);
		case CSS_VALUE_ThreeDFace:				return OP_RGB(0xDD, 0xDD, 0xDD);
		case CSS_VALUE_ThreeDHighlight:			return OP_RGB(0xDD, 0xDD, 0xDD);
		case CSS_VALUE_ThreeDLightShadow:		return OP_RGB(0xFF, 0xFF, 0xFF);
		case CSS_VALUE_ThreeDShadow:			return OP_RGB(0xAA, 0xAA, 0xAA);
		case CSS_VALUE_Window:					return OP_RGB(0xFF, 0xFF, 0xFF);
		case CSS_VALUE_WindowFrame:				return OP_RGB(0x00, 0x00, 0x00);
		case CSS_VALUE_WindowText:				return OP_RGB(0x00, 0x00, 0x00);
		default:								return OP_RGB(0xFF, 0xFF, 0xFF);
	}
}

BOOL MacOpUiInfo::GetUICSSFont(int css_font_value, FontAtt &font)
{
	switch (css_font_value)
	{
		case CSS_VALUE_caption:
		case CSS_VALUE_menu:
		case CSS_VALUE_message_box:
		case CSS_VALUE_small_caption:
		case CSS_VALUE_status_bar:
		case CSS_VALUE_icon:
			{
				font.SetFaceName(FONT_LUCIDA_GRANDE);
				font.SetSize((css_font_value==CSS_VALUE_caption||css_font_value==CSS_VALUE_menu)?13:11);
				font.SetWeight(4);
			}
			return TRUE;
	}
	return FALSE;
}

BOOL MacOpUiInfo::IsMouseRightHanded()
{
//#warning "Implement MacOpUiInfo::IsMouseRightHanded"
// FIXME: Implement MacOpUiInfo::IsMouseRightHanded
	return TRUE;
}

BOOL MacOpUiInfo::IsFullKeyboardAccessActive()
{
	if (g_preferences_dirty)
	{
		CFPreferencesAppSynchronize(kCFPreferencesCurrentApplication);
		g_preferences_dirty = false;

		CFPropertyListRef keyb_uimode = CFPreferencesCopyAppValue(CFSTR("AppleKeyboardUIMode"), kCFPreferencesCurrentApplication);
		if (keyb_uimode)
		{
			if (CFGetTypeID(keyb_uimode) == CFStringGetTypeID())
			{
				m_full_keyboard_access = CFStringGetIntValue((CFStringRef)keyb_uimode);
			}
			else if (CFGetTypeID(keyb_uimode) == CFNumberGetTypeID())
			{
				CFNumberGetValue((CFNumberRef)keyb_uimode, kCFNumberIntType, &m_full_keyboard_access);
			}
			CFRelease(keyb_uimode);
		}
	}

	return m_full_keyboard_access != 0;
}

UINT32 MacOpUiInfo::GetCaretWidth()
{
	return 1;
}

OP_STATUS MacOpUiInfo::SetCustomHighlightColor(BOOL use_custom, COLORREF new_color)
{
	m_use_custom_highlight = use_custom;
	m_custom_highlight = new_color;
	return OpStatus::OK;
}

