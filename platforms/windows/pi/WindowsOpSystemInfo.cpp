/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "adjunct/desktop_util/resources/ResourceDefines.h"
#include "adjunct/desktop_util/resources/pi/opdesktopproduct.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "adjunct/quick/managers/KioskManager.h"
#include "adjunct/quick/hotlist/hotlistparser.h"

#include "modules/display/styl_man.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/pi/OpBitmap.h"
#include "modules/pi/OpKeys.h"
#include "modules/prefs/prefsmanager/collections/pc_app.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefs/prefsmanager/collections/pc_mswin.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/style/src/css_values.h"
#include "modules/util/filefun.h"
#include "modules/util/gen_str.h"

#include "platforms/windows/ExternalDownloadManager.h"
#include "platforms/windows/Netfw.h"
#include "platforms/windows/user.h"
#include "platforms/windows/user_fun.h"
#include "platforms/windows/win_handy.h"
#include "platforms/windows/installer/OperaInstaller.h"
#include "platforms/windows/pi/WindowsOpDesktopResources.h"
#include "platforms/windows/pi/WindowsOpSystemInfo.h"
#include "platforms/windows/pi/WindowsOpWindow.h"
#include "platforms/windows/utils/win_icon.h"
#include "platforms/windows/windows_ui/exec.h"
#include "platforms/windows/windows_ui/Registry.h"
#include "platforms/windows/windows_ui/winshell.h"
#if defined(_PLUGIN_SUPPORT_) && defined(PI_PLUGIN_DETECT)
# include "platforms/windows/WindowsPluginDetector.h"
#endif // _PLUGIN_SUPPORT_ && PI_PLUGIN_DETECT

#include <Lmcons.h>

#include <intrin.h>
#include "adjunct/desktop_util/filelogger/desktopfilelogger.h"

// from winuser.h
#ifndef FE_FONTSMOOTHINGCLEARTYPE
#define FE_FONTSMOOTHINGCLEARTYPE           0x0002 
#endif

#ifndef SPI_GETFONTSMOOTHINGTYPE
#define SPI_GETFONTSMOOTHINGTYPE            0x200A
#endif

// To check Java plug-in version
#define IS_SUPPORTED(maj, min) (maj == 1 && min >= 2 || maj > 1)

OP_STATUS OpUiInfo::Create(OpUiInfo** new_opuiinfo)
{
	OP_ASSERT(new_opuiinfo != NULL);
	*new_opuiinfo = OP_NEW(WindowsOpSystemInfo,());
	if(*new_opuiinfo == NULL)
		return OpStatus::ERR_NO_MEMORY;
	return OpStatus::OK;
}

OP_STATUS OpSystemInfo::Create(OpSystemInfo** new_systeminfo)
{
	OP_ASSERT(new_systeminfo != NULL);
	*new_systeminfo = OP_NEW(WindowsOpSystemInfo,());
	if(*new_systeminfo == NULL)
		return OpStatus::ERR_NO_MEMORY;
	return OpStatus::OK;
}

WindowsOpSystemInfo::WindowsOpSystemInfo()
		: m_first(TRUE)
		, m_cd_letter(0)
		, m_screenreader_running(FALSE)
		, m_hardware_fingerprint_generated(FALSE)
{
	// FIXME::OOM-KILSMO
	main_thread_id = ::GetCurrentThreadId();
	m_system_encoding = NULL;
#ifdef EMBROWSER_SUPPORT
	m_use_custom_highlight_color = FALSE;
#endif // EMBROWSER_SUPPORT
	m_menu_font = NULL;
	m_os_ver[0] = 0;
}

WindowsOpSystemInfo::~WindowsOpSystemInfo()
{
	OnSettingsChanged();
	OP_DELETEA(m_system_encoding);
}

// Assumption: OpFileLength in Windows is always 64-bit (unsigned __int64 actually)
OP_STATUS WindowsOpSystemInfo::OpFileLengthToString(OpFileLength length, OpString8* result)
{
	char buf[24];

	sprintf(buf, "%I64u", length);

	return result->Set(buf);
}

OP_STATUS WindowsOpSystemInfo::StringToOpFileLength(const char* length, OpFileLength* result)
{
	// special hack to handle "0" as input string as _atoi64() returns 0 on error
	BOOL found_zeros = FALSE;
	const char *ptr = length;

	// strip leading spaces
	while(isspace((int)(unsigned char)*ptr))
		ptr++;

	if (*ptr == '-') // A file length can never be negative.
		return OpStatus::ERR;

	// check for zeros
	while(*ptr == '0')
	{
		found_zeros = TRUE;
		ptr++;
	}
	if(found_zeros && !isdigit(*ptr))
	{
		// all zeros, we're good
		*result = 0;
		return OpStatus::OK;
	}
	__int64 temp = _atoi64(length);
	if(temp < 1)
	{
		*result = 0;
		return OpStatus::ERR;
	}
	if((temp == _I64_MIN || temp == _I64_MAX) && errno == ERANGE)	// minus or 0 is an error, errno = ERANGE for all other cases
	{
		*result = 0;
		return OpStatus::ERR;
	}
	*result = (OpFileLength)temp;

	return OpStatus::OK;
}

UINT32 WindowsOpSystemInfo::GetPhysicalMemorySizeMB()
{
	MEMORYSTATUSEX ms;
	ms.dwLength = sizeof(ms);

	GlobalMemoryStatusEx(&ms);

	return ms.ullTotalPhys / (1024 * 1024);
}

void WindowsOpSystemInfo::OnSettingsChanged()
{
	if (m_menu_font)
	{
		DeleteObject(m_menu_font);
		m_menu_font = NULL;
	}
}

UINT32 WindowsOpSystemInfo::GetVerticalScrollbarWidth()
{
	return GetSystemMetrics(SM_CXVSCROLL);
}

UINT32 WindowsOpSystemInfo::GetHorizontalScrollbarHeight()
{
	return GetSystemMetrics(SM_CYHSCROLL);
}

UINT32 WindowsOpSystemInfo::GetSystemColor(OP_SYSTEM_COLOR color)
{
	UINT32 retcolor;

	switch (color)
	{
		case OP_SYSTEM_COLOR_BUTTON:
			retcolor = GetSysColor(COLOR_BTNFACE);
			break;

		case OP_SYSTEM_COLOR_BUTTON_LIGHT:
			retcolor = GetSysColor(COLOR_BTNHIGHLIGHT);
			break;

		case OP_SYSTEM_COLOR_BUTTON_DARK:
			retcolor = GetSysColor(COLOR_BTNSHADOW);
			break;

		case OP_SYSTEM_COLOR_BUTTON_VERYDARK:
			retcolor = GetSysColor(COLOR_3DDKSHADOW);
			break;

		case OP_SYSTEM_COLOR_BUTTON_TEXT:
			retcolor = GetSysColor(COLOR_BTNTEXT);
			break;


		case OP_SYSTEM_COLOR_BACKGROUND:
			retcolor = GetSysColor(COLOR_WINDOW);
			break;

		case OP_SYSTEM_COLOR_BACKGROUND_SELECTED:
#ifdef EMBROWSER_SUPPORT
			if (m_use_custom_highlight_color)
			{
				retcolor = m_custom_highlight_color;
			}
			else
#endif // EMBROWSER_SUPPORT
			{
				retcolor = GetSysColor(COLOR_HIGHLIGHT);
			}
			break;

		case OP_SYSTEM_COLOR_BACKGROUND_SELECTED_NOFOCUS:
			retcolor = GetSysColor(COLOR_BTNFACE);
			break;

		case OP_SYSTEM_COLOR_BACKGROUND_DISABLED:
			retcolor = GetSysColor(COLOR_BTNFACE);
			break;


		case OP_SYSTEM_COLOR_TEXT:
		case OP_SYSTEM_COLOR_TEXT_INPUT:
		case OP_SYSTEM_COLOR_DOCUMENT_PRE:
			retcolor = GetSysColor(COLOR_WINDOWTEXT);
			break;

		case OP_SYSTEM_COLOR_TEXT_SELECTED:
			retcolor = GetSysColor(COLOR_HIGHLIGHTTEXT);
			break;

		case OP_SYSTEM_COLOR_TEXT_SELECTED_NOFOCUS:
			retcolor = GetSysColor(COLOR_BTNTEXT);
			break;

		case OP_SYSTEM_COLOR_TEXT_HIGHLIGHTED:
			retcolor = OP_RGB(255, 255, 255);
			break;

		case OP_SYSTEM_COLOR_TEXT_HIGHLIGHTED_NOFOCUS:
			retcolor = OP_RGB(0, 0, 0);
			break;

		case OP_SYSTEM_COLOR_BACKGROUND_HIGHLIGHTED:
			retcolor = OP_RGB(35, 220, 35);		//Green
			break;

		case OP_SYSTEM_COLOR_BACKGROUND_HIGHLIGHTED_NOFOCUS:
			retcolor = OP_RGB(255, 255, 0);	//Yellow
			break;

		case OP_SYSTEM_COLOR_TEXT_DISABLED:
			retcolor = GetSysColor(COLOR_GRAYTEXT);
			break;

		case OP_SYSTEM_COLOR_SCROLLBAR_BACKGROUND:
			{
				// We need the light gray used on scrollbars, but it's not defined in the API. (Probably because its
				// dithered pixels between white and this color).
				UINT32 col = GetSysColor(COLOR_SCROLLBAR);
				INT r = GetRValue(col);
				INT g = GetGValue(col);
				INT b = GetBValue(col);
				r += (255 - r)/2;
				g += (255 - g)/2;
				b += (255 - b)/2;
				retcolor = OP_RGB(r, g, b);
			}
			break;

		case OP_SYSTEM_COLOR_DOCUMENT_NORMAL:
		case OP_SYSTEM_COLOR_DOCUMENT_HEADER1:
		case OP_SYSTEM_COLOR_DOCUMENT_HEADER2:
		case OP_SYSTEM_COLOR_DOCUMENT_HEADER3:
		case OP_SYSTEM_COLOR_DOCUMENT_HEADER4:
		case OP_SYSTEM_COLOR_DOCUMENT_HEADER5:
		case OP_SYSTEM_COLOR_DOCUMENT_HEADER6:
			retcolor = GetSysColor(COLOR_WINDOWTEXT);
			break;

		case OP_SYSTEM_COLOR_UI_FONT:
			retcolor = GetSysColor(COLOR_BTNTEXT);
			break;

		case OP_SYSTEM_COLOR_UI_DISABLED_FONT:
			retcolor = GetSysColor(COLOR_GRAYTEXT);
			break;

		case OP_SYSTEM_COLOR_UI_BACKGROUND:
			retcolor = GetSysColor(COLOR_BTNFACE);
			break;

		case OP_SYSTEM_COLOR_UI_BUTTON_BACKGROUND:
			retcolor = GetSysColor(COLOR_BTNHIGHLIGHT);
			break;

		case OP_SYSTEM_COLOR_UI_PROGRESS:
			retcolor = GetSysColor(COLOR_HIGHLIGHT);
			break;

		case OP_SYSTEM_COLOR_UI_HOTLIST_TREE_FONT:
			retcolor = GetSysColor(COLOR_WINDOWTEXT);
			break;

		case OP_SYSTEM_COLOR_LINK:
			retcolor = OP_RGB(0, 0, 204);
			break;

		case OP_SYSTEM_COLOR_VISITED_LINK:
			retcolor = OP_RGB(128, 0, 128);
			break;

		case OP_SYSTEM_COLOR_DOCUMENT_BACKGROUND:
			retcolor = GetSysColor(COLOR_WINDOW);
			break;

		case OP_SYSTEM_COLOR_UI_WINBAR_ATTENTION:
			retcolor = OP_RGB(0, 0, 204);
			break;

		case OP_SYSTEM_COLOR_UI_BUTTON_HOVER:
			retcolor = OP_RGB(170, 0, 0);
			break;

		case OP_SYSTEM_COLOR_SKIN:
			retcolor = GetSysColor(COLOR_BTNFACE);
			break;

		case OP_SYSTEM_COLOR_WORKSPACE:
			retcolor = GetSysColor(COLOR_APPWORKSPACE);
			break;

		default:
			OP_ASSERT(FALSE); // Implementation missing
			return 0;
	};
	return OP_RGB(OP_GET_R_VALUE(retcolor), OP_GET_G_VALUE(retcolor), OP_GET_B_VALUE(retcolor));
}

void WindowsOpSystemInfo::GetSystemFont(OP_SYSTEM_FONT font, FontAtt &retval)
{
	LOGFONT dlf;

	SetFontDefault(&dlf);

	dlf.lfCharSet = DEFAULT_CHARSET;

	if (m_first)
	{
		m_first = FALSE;

		HDC dc = GetDC(g_main_hwnd);
		HFONT hFont = (HFONT)GetStockObject(ANSI_FIXED_FONT);
		HFONT hOldFont = (HFONT)SelectObject(dc, hFont);

		m_height8 = MulDiv(8, GetDeviceCaps(dc, LOGPIXELSY), 72);   // returns 11
		m_height10 = MulDiv(10, GetDeviceCaps(dc, LOGPIXELSY), 72); // 13
		m_height12 = MulDiv(12, GetDeviceCaps(dc, LOGPIXELSY), 72); // 16
		SelectObject(dc, hOldFont);
		ReleaseDC(g_main_hwnd, dc);
	}

	switch (font)
	{
	
		case OP_SYSTEM_FONT_DOCUMENT_NORMAL:
			dlf.lfHeight  = -m_height12;
			dlf.lfWeight = 400;
			break;
		case OP_SYSTEM_FONT_DOCUMENT_HEADER1:
			dlf.lfHeight = -m_height12 - 12;
			dlf.lfWeight = 700;
			break;
		case OP_SYSTEM_FONT_UI_HEADER:
		case OP_SYSTEM_FONT_DOCUMENT_HEADER2:
			dlf.lfHeight = -m_height12 - 6;
			dlf.lfWeight = 700;
			break;
		case OP_SYSTEM_FONT_DOCUMENT_HEADER3:
			dlf.lfHeight = -m_height12 - 2;
			dlf.lfWeight = 700;
			break;
		case OP_SYSTEM_FONT_DOCUMENT_HEADER4:
			dlf.lfHeight = -m_height12 - 0;
			dlf.lfWeight = 700;
			break;
		case OP_SYSTEM_FONT_DOCUMENT_HEADER5:
			dlf.lfHeight = -m_height12 + 2;
			dlf.lfWeight = 700;
			break;
		case OP_SYSTEM_FONT_DOCUMENT_HEADER6:
			dlf.lfHeight = -m_height12 + 4;
			dlf.lfWeight = 700;
			break;
		case OP_SYSTEM_FONT_DOCUMENT_PRE:
			dlf.lfHeight = -m_height10;
			dlf.lfWeight = 400;
			break;
		case OP_SYSTEM_FONT_FORM_TEXT_INPUT:
		case OP_SYSTEM_FONT_FORM_BUTTON:
		case OP_SYSTEM_FONT_FORM_TEXT:
		case OP_SYSTEM_FONT_UI_TOOLBAR:
		case OP_SYSTEM_FONT_UI_DIALOG:
		case OP_SYSTEM_FONT_UI_PANEL:
		case OP_SYSTEM_FONT_UI_MENU:
		case OP_SYSTEM_FONT_UI_TOOLTIP:
			dlf.lfHeight = -m_height10;
			dlf.lfWeight = 400;
			break;

		case OP_SYSTEM_FONT_UI:
		case OP_SYSTEM_FONT_UI_DISABLED:
		case OP_SYSTEM_FONT_UI_HOTLIST_TREE:
			dlf.lfHeight = -m_height8;
			dlf.lfWeight = 400;
			break;
		case OP_SYSTEM_FONT_EMAIL_COMPOSE:
		case OP_SYSTEM_FONT_HTML_COMPOSE:
		case OP_SYSTEM_FONT_EMAIL_DISPLAY:
			dlf.lfHeight = -m_height10;
			dlf.lfWeight = 400;
			break;
		default:
			return;
	}

	OpFontManager* font_manager= styleManager->GetFontManager();

	switch (font)
	{
	case OP_SYSTEM_FONT_DOCUMENT_PRE:
	case OP_SYSTEM_FONT_FORM_TEXT:
		uni_strlcpy(dlf.lfFaceName, font_manager->GetGenericFontName(GENERIC_FONT_MONOSPACE), LF_FACESIZE-1);
		dlf.lfPitchAndFamily = 49;
		break;

	case OP_SYSTEM_FONT_FORM_BUTTON:
	case OP_SYSTEM_FONT_FORM_TEXT_INPUT:
		uni_strlcpy(dlf.lfFaceName, font_manager->GetGenericFontName(GENERIC_FONT_SANSSERIF), LF_FACESIZE-1);
		dlf.lfPitchAndFamily = 34;
		break;

	case OP_SYSTEM_FONT_UI_TOOLBAR:
	case OP_SYSTEM_FONT_UI_DIALOG:
	case OP_SYSTEM_FONT_UI_PANEL:
		g_op_ui_info->GetUICSSFont(CSS_VALUE_message_box, retval);
		return;
	case OP_SYSTEM_FONT_UI_HEADER:
		g_op_ui_info->GetUICSSFont(CSS_VALUE_message_box, retval);
		retval.SetSize(24);
		return;
	case OP_SYSTEM_FONT_UI_MENU:
		g_op_ui_info->GetUICSSFont(CSS_VALUE_menu, retval);
		return;

	case OP_SYSTEM_FONT_UI_TOOLTIP:
		g_op_ui_info->GetUICSSFont(CSS_VALUE_status_bar, retval);
		return;
	case OP_SYSTEM_FONT_EMAIL_COMPOSE:
	case OP_SYSTEM_FONT_EMAIL_DISPLAY:
		uni_strlcpy(dlf.lfFaceName, font_manager->GetGenericFontName(GENERIC_FONT_MONOSPACE), LF_FACESIZE-1);
		break;

	default:
		//julienp: See bugs 253326 and 135518
		WritingSystem::Script script = WritingSystem::FromEncoding(g_op_system_info->GetSystemEncodingL());
		if (script == WritingSystem::Japanese || script == WritingSystem::Korean)
			uni_strncpy(dlf.lfFaceName, font_manager->GetGenericFontName(GENERIC_FONT_SANSSERIF), LF_FACESIZE);
		else
			uni_strlcpy(dlf.lfFaceName, font_manager->GetGenericFontName(GENERIC_FONT_SERIF), LF_FACESIZE-1);
		dlf.lfPitchAndFamily = 18;
		break;
	}

	LogFontToFontAtt(dlf, retval);
}

void WindowsOpSystemInfo::GetFont(OP_SYSTEM_FONT font, FontAtt &retval, BOOL use_system_default)
{
	if (use_system_default)
	{
		if (font == OP_SYSTEM_FONT_EMAIL_COMPOSE ||
			font == OP_SYSTEM_FONT_EMAIL_DISPLAY)
		{
#ifdef DISPLAY_CAP_STYLE_IS_LAYOUTSTYLE
			LayoutStyle *s = styleManager->GetStyleEx(STYLE_MAIL_BODY);
#else // DISPLAY_CAP_STYLE_IS_LAYOUTSTYLE
			Style *s = styleManager->GetStyleEx(STYLE_MAIL_BODY);
#endif // DISPLAY_CAP_STYLE_IS_LAYOUTSTYLE
			PresentationAttr p = s->GetPresentationAttr();
			retval = *(p.GetPresentationFont(StringUtils::GetDefaultWritingSystem()).Font);		
		}
		else if (font == OP_SYSTEM_FONT_UI_MENU)
		{
			GetUICSSFont(CSS_VALUE_menu, retval);
		}
		else if (font == OP_SYSTEM_FONT_UI_TOOLTIP)
		{
			GetUICSSFont(CSS_VALUE_status_bar, retval);
		}
		else
		{
			GetUICSSFont(CSS_VALUE_message_box, retval);
		}
		return;
	}

	g_pcfontscolors->GetFont(font, retval);
}

COLORREF WindowsOpSystemInfo::GetUICSSColor(int css_color_value)
{
	COLORREF color;

	switch (css_color_value)
	{
		case CSS_VALUE_ActiveBorder:			color = GetSysColor(COLOR_ACTIVEBORDER);		break;
		case CSS_VALUE_ActiveCaption:			color = GetSysColor(COLOR_ACTIVECAPTION);		break;
		case CSS_VALUE_AppWorkspace:			color = GetSysColor(COLOR_APPWORKSPACE);		break;
		case CSS_VALUE_Background:				color = GetSysColor(COLOR_BACKGROUND);			break;
		case CSS_VALUE_ButtonHighlight:			color = GetSysColor(COLOR_BTNHIGHLIGHT);		break;
		case CSS_VALUE_ButtonShadow:			color = GetSysColor(COLOR_BTNSHADOW);			break;
		case CSS_VALUE_ButtonText:				color = GetSysColor(COLOR_BTNTEXT);				break;
		case CSS_VALUE_CaptionText:				color = GetSysColor(COLOR_CAPTIONTEXT);			break;
		case CSS_VALUE_GrayText:				color = GetSysColor(COLOR_GRAYTEXT);			break;
		case CSS_VALUE_Highlight:				color = GetSysColor(COLOR_HIGHLIGHT);			break;
		case CSS_VALUE_HighlightText:			color = GetSysColor(COLOR_HIGHLIGHTTEXT);		break;
		case CSS_VALUE_InactiveBorder:			color = GetSysColor(COLOR_INACTIVEBORDER);		break;
		case CSS_VALUE_InactiveCaption:			color = GetSysColor(COLOR_INACTIVECAPTION);		break;
		case CSS_VALUE_InactiveCaptionText:		color = GetSysColor(COLOR_INACTIVECAPTIONTEXT);	break;
		case CSS_VALUE_InfoBackground:			color = GetSysColor(COLOR_INFOBK);				break;
		case CSS_VALUE_InfoText:				color = GetSysColor(COLOR_INFOTEXT);			break;
		case CSS_VALUE_Menu:					color = GetSysColor(COLOR_MENU);				break;
		case CSS_VALUE_MenuText:				color = GetSysColor(COLOR_MENUTEXT);			break;
		case CSS_VALUE_Scrollbar:				color = GetSysColor(COLOR_SCROLLBAR);			break;
		case CSS_VALUE_ThreeDDarkShadow:		color = GetSysColor(COLOR_3DDKSHADOW);			break;
		case CSS_VALUE_ThreeDFace:				color = GetSysColor(COLOR_3DFACE);				break;
		case CSS_VALUE_ThreeDHighlight:			color = GetSysColor(COLOR_3DHIGHLIGHT);			break;
		case CSS_VALUE_ThreeDLightShadow:		color = GetSysColor(COLOR_3DLIGHT);				break;
		case CSS_VALUE_ThreeDShadow:			color = GetSysColor(COLOR_3DSHADOW);			break;
		case CSS_VALUE_Window:					color = GetSysColor(COLOR_WINDOW);				break;
		case CSS_VALUE_WindowFrame:				color = GetSysColor(COLOR_WINDOWFRAME);			break;
		case CSS_VALUE_WindowText:				color = GetSysColor(COLOR_WINDOWTEXT);			break;
		default:								color = GetSysColor(COLOR_3DFACE);				break;
	}
	return OP_RGB(OP_GET_R_VALUE(color), OP_GET_G_VALUE(color), OP_GET_B_VALUE(color));
}

BOOL WindowsOpSystemInfo::GetUICSSFont(int css_font_value, FontAtt &font)
{
	if (css_font_value == CSS_VALUE_icon)
	{
		LOGFONT lf;
		SystemParametersInfo(SPI_GETICONTITLELOGFONT, sizeof(LOGFONT), &lf, 0);
		LogFontToFontAtt(lf, font);
		return TRUE;
	}
	else
	{
		NONCLIENTMETRICS ncm;

		ncm.cbSize = sizeof(NONCLIENTMETRICS);

		SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0);

		LOGFONT lf;

		switch (css_font_value)
		{
			case CSS_VALUE_caption:			lf = ncm.lfCaptionFont;		break;
			case CSS_VALUE_menu:			lf = ncm.lfMenuFont;			break;
			case CSS_VALUE_message_box:		lf = ncm.lfMessageFont;		break;
			case CSS_VALUE_small_caption:	lf = ncm.lfSmCaptionFont, font;	break;
			case CSS_VALUE_status_bar:		lf = ncm.lfStatusFont;		break;
			default:						return FALSE;
		}

		// if the selected system font doesn't support ChineseSimplified, switch to a usable one instead of relying on core fontswitching.
		// As core will switch to a very ugly font.
		OpString language_file;
		OpFile current_lng_file;

		TRAPD(rc, g_pcfiles->GetFileL(PrefsCollectionFiles::LanguageFile, current_lng_file));
		language_file.Set(current_lng_file.GetName());

		// This is not very useful now that DSK-239292 is done.
		if(language_file.FindI("zh-cn") !=KNotFound )
		{
			OpFontInfo specified_font;
			short font_num = styleManager->GetFontNumber(lf.lfFaceName);
			if (font_num && OpStatus::IsSuccess(styleManager->GetFontManager()->GetFontInfo(font_num, &specified_font)) )
			{
				if (!specified_font.HasScript(WritingSystem::ChineseSimplified))
				{
					BOOL bfound = TRUE;
					BOOL clear_type = FALSE;

					if (IsSystemWinXP()) // xp,vista,7
					{
						BOOL font_smoothing = FALSE;
						if(SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &font_smoothing, 0) && font_smoothing)
						{
							UINT smoothing_type = 0;
							if (SystemParametersInfo(SPI_GETFONTSMOOTHINGTYPE, 0, &smoothing_type, 0))
								clear_type = smoothing_type == FE_FONTSMOOTHINGCLEARTYPE;
						}
					}

					// Don't use Microsoft Yahei for menu. Opera has problems rendering menu using this font.
					if ( clear_type && css_font_value != CSS_VALUE_menu && styleManager->GetFontNumber(UNI_L("\x5fae\x8f6f\x96c5\x9ed1")) != -1)
						uni_strncpy(lf.lfFaceName, UNI_L("\x5fae\x8f6f\x96c5\x9ed1"), 5 );
					else if(clear_type && css_font_value != CSS_VALUE_menu && styleManager->GetFontNumber(UNI_L("Microsoft YaHei")) != -1)
						uni_strncpy(lf.lfFaceName, UNI_L("Microsoft YaHei"), 16 );	
					else if (styleManager->GetFontNumber(UNI_L("\x5B8B\x4F53")) != -1)
						uni_strncpy(lf.lfFaceName, UNI_L("\x5B8B\x4F53"), 3 );
					else if(styleManager->GetFontNumber(UNI_L("SimSun")) != -1)
						uni_strncpy(lf.lfFaceName, UNI_L("SimSun"), 7 );
					else 
						bfound = FALSE;

					if(!bfound)
					{
						GetObject(GetStockObject(DEFAULT_GUI_FONT),sizeof(lf),&lf);
					}
				}
			}
		}

		LogFontToFontAtt(lf, font);
		//Hack for Windows 95/98: The default MS Sans Serif font scales badly.
		//We want something that scales better in general and Arial seems most suited
		//(Tahoma was not provided before Win98)
		if (!IsSystemWin2000orXP() && !uni_strncmp(lf.lfFaceName, UNI_L("MS Sans Serif"), 14))
		{
			font.SetFaceName(UNI_L("Arial"));
		}
		return TRUE;
	}
}

BOOL WindowsOpSystemInfo::IsMouseRightHanded()
{
	// TODO WindowsOpSystemInfo::IsMouseRightHanded should be implemented
	return TRUE;
}

void WindowsOpSystemInfo::GetAutoProxyURLL(OpString *url, BOOL *enabled)
{
	DWORD size;
	*enabled = FALSE;
	if (OpRegReadStrValue(HKEY_CURRENT_USER, UNI_L("Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings"), UNI_L("AutoConfigURL"), NULL, &size) == ERROR_SUCCESS)
	{
		if (!url->Reserve(size/sizeof(uni_char)) ||
			OpRegReadStrValue(HKEY_CURRENT_USER, UNI_L("Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings"), UNI_L("AutoConfigURL"), url->CStr(), &size) != ERROR_SUCCESS ||
			url->IsEmpty())
			return;

		*enabled = TRUE;
	}		
}

void WindowsOpSystemInfo::GetProxyExceptionsL(OpString *exceptions, BOOL *enabled)
{
	// TODO WindowsOpSystemInfo::GetProxyExceptionsL should be implemented
}

void WindowsOpSystemInfo::GetProxySettingsL(
	const uni_char *protocol, int &enabled, OpString &proxyserver)
{
// HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Internet Settings -> ProxyServer
// will give you something like this:
//  "ftp=cdwriter.3opera.com:1080;gopher=cdwriter.op4era.com:1080;http=cdwriter.opera.com:1080;https=cdwriter2.opera.com:1080;socks=cdwriter.opera.com3:1"
// or if all protocols use the same server
//  "cdwriter.opera.com:1080"

	OpString proxyvalue;
	DWORD size;
	enabled = FALSE;
	proxyserver.Empty();

	//WAIS is not stored in registry
	if((protocol && uni_strni_eq(protocol, "WAIS", 5)) || proxyvalue.ReserveL(2048) == NULL)
	{
		return;
	}

	//SOCKS is not read from IE settings
	if(protocol && uni_strni_eq(protocol, "SOCKS", 6))
	{
		return;
	}

	size = proxyvalue.Capacity();

	if (OpRegReadStrValue(HKEY_CURRENT_USER, UNI_L("Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings"), UNI_L("ProxyServer"), proxyvalue, &size) == ERROR_SUCCESS)
	{
		if(proxyvalue.IsEmpty())
		{
			return;
		}

		OpString matchpattern;
		LEAVE_IF_ERROR(matchpattern.AppendFormat(UNI_L("%s="), protocol));
		
		int pos = proxyvalue.FindI(matchpattern);
		
		if(pos != KNotFound)
		{
			// Protocol specific setting
			int endpos = proxyvalue.SubString(pos+matchpattern.Length()).FindFirstOf(';');
			if(endpos != KNotFound)
			{
				proxyserver.SetL(proxyvalue.SubString(pos+matchpattern.Length(), endpos));
			}
			else
			{
				proxyserver.SetL(proxyvalue.SubString(pos+matchpattern.Length()));
			}
		}
		else if (proxyvalue.Find(UNI_L("=")) == KNotFound)
		{
			// One proxy setting to rule them all
			proxyserver.SetL(proxyvalue);
		}
		else
		{
			return;
		}

		DWORD proxy_enabled;

		if (!proxyserver.IsEmpty() &&
			OpRegReadDWORDValue(HKEY_CURRENT_USER,
					UNI_L("Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings"),
					UNI_L("ProxyEnable"), &proxy_enabled) == ERROR_SUCCESS)
		{
			enabled = proxy_enabled;
		}
	}
}

#ifdef _PLUGIN_SUPPORT_
void WindowsOpSystemInfo::GetPluginPathL(const OpStringC &dfpath, OpString &newpath)
{
	//
	//	Get from Opera.ini
	//
	OpString tmppluginpath; ANCHOR(OpString, tmppluginpath);
	tmppluginpath.SetL(dfpath);

	OpString path; ANCHOR(OpString, path);
	LEAVE_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_RESOURCES_FOLDER, path));
	path.AppendL("program"PATHSEP"plugins");

	if (tmppluginpath.HasContent())
		tmppluginpath.AppendL(";");
	tmppluginpath.AppendL(path);

	if (OpStatus::IsSuccess(OpSafeRegQueryValue(HKEY_CURRENT_USER, UNI_L("Software\\Opera Software"), path, UNI_L("Plugin path"))) && path.HasContent())
	{
		tmppluginpath.AppendL(";");
		tmppluginpath.AppendL(path);
	}

	if (OpStatus::IsSuccess(OpSafeRegQueryValue(HKEY_CURRENT_USER, UNI_L("Software\\Opera Software"), path, UNI_L("Next Plugin path"))) && path.HasContent())
	{
		tmppluginpath.AppendL(";");
		tmppluginpath.AppendL(path);
	}

	OpString version; ANCHOR(OpString, version);
	if (OpStatus::IsSuccess(OpSafeRegQueryValue(HKEY_LOCAL_MACHINE, UNI_L("SOFTWARE\\Mozilla\\Mozilla Firefox"), version, UNI_L("CurrentVersion"))))
	{
		version.InsertL(0, "SOFTWARE\\Mozilla\\Mozilla Firefox\\");
		version.AppendL("\\Main");
		if (OpStatus::IsSuccess(OpSafeRegQueryValue(HKEY_LOCAL_MACHINE, version.CStr(), path, UNI_L("Install Directory"))) && path.HasContent())
		{
			path.AppendL(PATHSEP"plugins");
			tmppluginpath.AppendL(";");
			tmppluginpath.AppendL(path);
		}
	}

	if (OpStatus::IsSuccess(OpSafeRegQueryValue(HKEY_LOCAL_MACHINE, UNI_L("SOFTWARE\\Netscape\\Netscape Navigator"), version, UNI_L("CurrentVersion"))) && version.CompareI("Opera") != 0)
	{
		version.InsertL(0, "SOFTWARE\\Mozilla\\Mozilla Firefox\\");
		version.AppendL("\\Main");
		if (OpStatus::IsSuccess(OpSafeRegQueryValue(HKEY_LOCAL_MACHINE, version.CStr(), path, UNI_L("Install Directory"))) && path.HasContent())
		{
			path.AppendL(PATHSEP"plugins");
			tmppluginpath.AppendL(";");
			tmppluginpath.AppendL(path);
		}
	}

	if (OpStatus::IsSuccess(OpSafeRegQueryValue(HKEY_CURRENT_USER, UNI_L("Software\\Netscape\\Netscape Navigator\\Main"), path, UNI_L("Install Directory"))) && path.HasContent())
	{
		path.AppendL(PATHSEP"plugins");
		tmppluginpath.AppendL(";");
		tmppluginpath.AppendL(path);
	}

	if (OpStatus::IsSuccess(OpSafeRegQueryValue(HKEY_LOCAL_MACHINE, UNI_L("SOFTWARE\\Microsoft\\MediaPlayer"), path, UNI_L("Installation Directory"))) && path.HasContent())
	{
		tmppluginpath.AppendL(";");
		tmppluginpath.AppendL(path);
	}

#ifdef _WIN64
	if (OpStatus::IsSuccess(OpSafeRegQueryValue(HKEY_CURRENT_USER, UNI_L("Software\\Opera Software"), path, UNI_L("Plugin path"), KEY_WOW64_32KEY)) && path.HasContent())
	{
		tmppluginpath.AppendL(";");
		tmppluginpath.AppendL(path);
	}

	if (OpStatus::IsSuccess(OpSafeRegQueryValue(HKEY_CURRENT_USER, UNI_L("Software\\Opera Software"), path, UNI_L("Next Plugin path"), KEY_WOW64_32KEY)) && path.HasContent())
	{
		tmppluginpath.AppendL(";");
		tmppluginpath.AppendL(path);
	}

	if (OpStatus::IsSuccess(OpSafeRegQueryValue(HKEY_LOCAL_MACHINE, UNI_L("SOFTWARE\\Mozilla\\Mozilla Firefox"), version, UNI_L("CurrentVersion"), KEY_WOW64_32KEY)))
	{
		version.InsertL(0, "SOFTWARE\\Mozilla\\Mozilla Firefox\\");
		version.AppendL("\\Main");
		if (OpStatus::IsSuccess(OpSafeRegQueryValue(HKEY_LOCAL_MACHINE, version.CStr(), path, UNI_L("Install Directory"), KEY_WOW64_32KEY)) && path.HasContent())
		{
			path.AppendL(PATHSEP"plugins");
			tmppluginpath.AppendL(";");
			tmppluginpath.AppendL(path);
		}
	}

	if (OpStatus::IsSuccess(OpSafeRegQueryValue(HKEY_LOCAL_MACHINE, UNI_L("SOFTWARE\\Netscape\\Netscape Navigator"), version, UNI_L("CurrentVersion"), KEY_WOW64_32KEY)) && version.CompareI("Opera") != 0)
	{
		version.InsertL(0, "SOFTWARE\\Mozilla\\Mozilla Firefox\\");
		version.AppendL("\\Main");
		if (OpStatus::IsSuccess(OpSafeRegQueryValue(HKEY_LOCAL_MACHINE, version.CStr(), path, UNI_L("Install Directory"), KEY_WOW64_32KEY)) && path.HasContent())
		{
			path.AppendL(PATHSEP"plugins");
			tmppluginpath.AppendL(";");
			tmppluginpath.AppendL(path);
		}
	}

	if (OpStatus::IsSuccess(OpSafeRegQueryValue(HKEY_CURRENT_USER, UNI_L("Software\\Netscape\\Netscape Navigator\\Main"), path, UNI_L("Install Directory"), KEY_WOW64_32KEY)) && path.HasContent())
	{
		path.AppendL(PATHSEP"plugins");
		tmppluginpath.AppendL(";");
		tmppluginpath.AppendL(path);
	}

	if (OpStatus::IsSuccess(OpSafeRegQueryValue(HKEY_LOCAL_MACHINE, UNI_L("SOFTWARE\\Microsoft\\MediaPlayer"), path, UNI_L("Installation Directory"), KEY_WOW64_32KEY)) && path.HasContent())
	{
		tmppluginpath.AppendL(";");
		tmppluginpath.AppendL(path);
	}

#endif //_WIN64

/*
	HKLM\SOFTWARE\Mozilla\Mozilla Firefox\CurrentVersion -> FF_VERSION
HKLM\SOFTWARE\Netscape\Netscape Navigator\CurrentVersion -> NS_VERSION
if NS_VERSION !="Opera"
	HKLM\SOFTWARE\Netscape\Netscape Navigator\[NS_VERSION]\Main\Plugins Directory -> PLUGINS
HKLM\SOFTWARE\Mozilla\Mozilla Firefox\[FF_VERSION]\Main\Install Directory + "\plugins" -> PLUGINS
HKCU\Software\Netscape\Netscape Navigator\Main\Install Directory -> PLUGINS
HKLM\SOFTWARE\Microsoft\MediaPlayer\Installation Directory -> PLUGINS
*/
#ifdef _APPLET_2_EMBED_

	//
	// Sun/Oracle Java plug-in
	//
	
	OpString java_path;
	
	HKEY key;
	const DWORD java_home_size = 256;
	uni_char java_home[java_home_size];
	DWORD java_home_len;
	
	const DWORD key_name_size = 32;
	uni_char key_name[key_name_size];
	DWORD key_name_len;
	
	int major(0), minor(0);
	
	const uni_char sun_key_name[] = UNI_L("SOFTWARE\\JavaSoft\\Java Runtime Environment");
	
	if (ERROR_SUCCESS == RegOpenKeyEx(HKEY_LOCAL_MACHINE, sun_key_name, 0, KEY_ENUMERATE_SUB_KEYS, &key))
	{
		DWORD index = 0;
		LONG ret = ERROR_SUCCESS;

		while (ret != ERROR_NO_MORE_ITEMS)
		{
			key_name_len = key_name_size;
			ret = RegEnumKeyEx(key, index++, key_name, &key_name_len, NULL, NULL, NULL, NULL);
			if (ret == ERROR_SUCCESS)
			{
				ParseJavaVersionString(key_name, major, minor);

				if (IS_SUPPORTED(major, minor))
				{
					java_home_len = UNICODE_SIZE(java_home_size);
					if (OpRegReadStrValue(key, key_name, UNI_L("JavaHome"), &java_home[0], &java_home_len) == ERROR_SUCCESS)
					{
					    OpString path;
					    path.Set(&java_home[0]);
					    path.Append(UNI_L("\\bin\\new_plugin\\"));
					    
					    if (PathIsDirectory(path.CStr()))
							java_path.Set(path);

					}
				}
			}
		}

		RegCloseKey(key);
		
		if (tmppluginpath.HasContent())
			tmppluginpath.Append(";");
			
		tmppluginpath.Append(java_path);
	}
	
#endif // _APPLET_2_EMBED_
	
	//
	//	Clean up the path list.
	//
	uni_char *tokenArr[256];
	int tokenCount = GetStrTokens(tmppluginpath.CStr(), UNI_L(";"), UNI_L(" \t"), tokenArr, ARRAY_SIZE(tokenArr), TRUE);

	if (tokenCount > 0)
	{
		//	Scan for duplicates
		for (int i=0; i<tokenCount; i++)
			if (tokenArr[i])
				for (int ii=i+1; ii<tokenCount; ii++)
					if (tokenArr[ii] && uni_stricmp(tokenArr[i], tokenArr[ii]) == 0)
						tokenArr[ii] = NULL;	//	remove duplicate

		//	Rebuild the path string
		newpath.Empty();
		for (int i=0; i<tokenCount; i++)
		{
			// Remove local non-existing dirs and make sure path does not
			// overflow buffer
			if (IsStr(tokenArr[i]) && DirExist(tokenArr[i]) && !IsNetworkDrive(tokenArr[i]))
			{					
				newpath.AppendL(UNI_L(";"));
				newpath.AppendL(tokenArr[i]);
			}
		}
		if (newpath.HasContent())
			newpath.Delete(0, 1);
	}
}
#endif // _PLUGIN_SUPPORT_

#if defined(_PLUGIN_SUPPORT_) && defined(PI_PLUGIN_DETECT)
OP_STATUS WindowsOpSystemInfo::DetectPlugins(const OpStringC& suggested_plugin_paths, class OpPluginDetectionListener* listener)
{
	WindowsPluginDetector detector(listener);
	return detector.ReadPlugins(suggested_plugin_paths);
}
#endif // _PLUGIN_SUPPORT_ && PI_PLUGIN_DETECT

char WindowsOpSystemInfo::GetCDLetter()
{
	if (0 == m_cd_letter)
	{
		// Find the first CD-ROM of the system
		int nDriveType;

		m_cd_letter = 'C';

		char drive[4];
		if (_pgmptr && strlen(_pgmptr) > 3)
		{
			strncpy(drive, _pgmptr, 3);
			drive[3] = 0;
			nDriveType = GetDriveTypeA(drive);
			if (nDriveType == DRIVE_CDROM)
			{
				m_cd_letter = drive[0];
				return m_cd_letter;
			}
		}			
		else
			strcpy(drive, "c:\\");

		int i;		     	
		for (i = 25; i >= 0; i--)	//read trough the drive list
		{
			drive[0] = i + 'A';
			nDriveType = GetDriveTypeA(drive);
			if (nDriveType == DRIVE_CDROM)
			{
				m_cd_letter = i + 'A';
				return m_cd_letter;
			}
		}

		// Indicate that none was found, it is unlikely that the user adds
		// a cd-rom while Opera is running.
		m_cd_letter = 1;
	}

	return m_cd_letter >= 'A' ? m_cd_letter : 0;
}

BOOL WindowsOpSystemInfo::IsInMainThread()
{
	DWORD thread_id = ::GetCurrentThreadId();
	return (thread_id == main_thread_id);
}

const char *WindowsOpSystemInfo::GetSystemEncodingL()
{
	if (NULL == m_system_encoding)
	{
		switch (GetACP())
		{
		case 932: SetStrL(m_system_encoding, "iso-2022-jp"); break; // default encoding should not be shift_jis
		case 936: SetStrL(m_system_encoding, "gbk"); break;
		case 949: SetStrL(m_system_encoding, "euc-kr"); break;
		case 950: SetStrL(m_system_encoding, "big5"); break;
		default:
			m_system_encoding = OP_NEWA_L(char, 16);
			sprintf(m_system_encoding, "windows-%d", GetACP());
			break;
		}
	}

	return m_system_encoding;
}

const uni_char* WindowsOpSystemInfo::GetDefaultTextEditorL()
{
	m_system_text_editor.SetL("Opera");
	return m_system_text_editor.CStr();
}

#ifdef OPSYSTEMINFO_GETFILETYPENAME
OP_STATUS WindowsOpSystemInfo::GetFileTypeName(const uni_char *filename, uni_char *out, size_t out_max_len)
{
	SHFILEINFO fileInfo;
	DWORD res = SHGetFileInfo(filename, FILE_ATTRIBUTE_NORMAL,
		&fileInfo, sizeof(SHFILEINFO), SHGFI_TYPENAME|SHGFI_USEFILEATTRIBUTES);

	if (res && fileInfo.szTypeName[0])
	{
		if (uni_strncmp(fileInfo.szTypeName,UNI_L("Opera"),6))
			uni_strlcpy(out, fileInfo.szTypeName, out_max_len);
		else
		{
			OpString str;
			OpString ext;
			RETURN_IF_ERROR(g_languageManager->GetString(Str::S_ALT_FILE_TYPE_NAME, str));

			ext.Set(uni_strrchr(filename, '.')+1);
			uni_strupr(ext.CStr());
			uni_snprintf(out, out_max_len, str, ext.CStr());
		}
	}
	else
	{
		out[0] = 0;
	}
	return OpStatus::OK;
}
#endif

OP_STATUS WindowsOpSystemInfo::GetHandlerStrings(URLType type, const OpString& filename, const OpString& content_type, OpVector<OpString>& handlers)
{
	if (type ==URL_FILE)
	{
		const uni_char *filename_only = filename.CStr();
		int file_position = filename.FindLastOf('\\');
		if (file_position != KNotFound)
			filename_only += file_position+1;

		//if we have a file without extension, we don't know the handler
		if (filename.FindLastOf('.') < 0)
			return OpStatus::OK;

		OpAutoPtr<OpString> first_handler = OP_NEW(OpString, ());
		RETURN_OOM_IF_NULL(first_handler.get());

		RETURN_IF_ERROR(GetShellFileHandler(filename_only, *first_handler));

		if (first_handler->IsEmpty())
		{
			RETURN_IF_ERROR(GetAllShellFileHandlers(filename.CStr(), handlers, NULL));
		}
		else
		{
			RETURN_IF_ERROR(GetAllShellFileHandlers(filename.CStr(), handlers, first_handler.get()));
			RETURN_IF_ERROR(handlers.Insert(0, first_handler.get()));
			first_handler.release();
		}
	}
	else
	{
		RETURN_IF_ERROR(GetURLHandlers(handlers));
	}
	return OpStatus::OK;
}

OP_STATUS WindowsOpSystemInfo::GetNamesAndIconsForHandlers(const OpVector<OpString>& handlers, UINT32 icon_size, OpVector<OpString>& names, OpVector<OpBitmap>& icons)
{
	for (UINT32 i = 0; i< handlers.GetCount(); i++)
	{
		OpAutoPtr<OpString> file_description(OP_NEW(OpString, ()));
		RETURN_OOM_IF_NULL(file_description.get());

		const uni_char* handler = handlers.Get(i)->CStr();
		OpBitmap* handler_icon = NULL;

		if (OpStatus::IsError(GetFileHandlerInfo(handler, file_description.get(), &handler_icon, icon_size)))
		{
			if (!handler_icon)
			{
				GetShellDefaultIcon(UNI_L(".com"), &handler_icon, icon_size);
			}
		}
		OP_STATUS err;
		if (OpStatus::IsError(err=names.Add(file_description.get())))
		{
			OP_DELETE(handler_icon);
			return err;
		}
		file_description.release();
		if (OpStatus::IsError(err=icons.Add(handler_icon)))
		{
			OP_DELETE(handler_icon);
			return err;
		}
	}

	return OpStatus::OK;
}

OP_STATUS WindowsOpSystemInfo::GetFileHandler(const OpString* filename, OpString& contentType, OpString& handler)
{
	OpAutoVector<OpString> handlers;

	// can return success even though it didn't actually find a handler
	RETURN_IF_ERROR(GetHandlerStrings(URL_FILE, *filename, contentType, handlers));

	return handlers.GetCount() ? handler.Set(*(handlers.Get(0))) : OpStatus::ERR;
}

OP_STATUS WindowsOpSystemInfo::GetFileHandlers(const OpString& filename, const OpString &content_type, OpVector<OpString>& handlers, OpVector<OpString>& handler_names, OpVector<OpBitmap>& handler_icons, URLType type, UINT32 icon_size)
{
	RETURN_IF_ERROR(GetHandlerStrings(type, filename, content_type, handlers));
	return GetNamesAndIconsForHandlers(handlers, icon_size, handler_names, handler_icons);
}

OP_STATUS WindowsOpSystemInfo::GetFileTypeInfo(const OpStringC& filename, const OpStringC& content_type, OpString& content_type_name,
							OpBitmap *& content_type_bitmap, UINT32 content_type_bitmap_size)
{
	RETURN_IF_ERROR(GetShellDefaultIcon(filename.CStr(), &content_type_bitmap, content_type_bitmap_size));
	if (!content_type_name.Reserve(81))
		return OpStatus::ERR_NO_MEMORY;
	RETURN_IF_ERROR(GetFileTypeName(filename.CStr(), content_type_name.CStr(), 81));
	
//	RETURN_IF_ERROR(CreateIconBitmapForFile(&content_type_bitmap, filename.CStr()));
	return OpStatus::OK;
}


OP_STATUS WindowsOpSystemInfo::GetProtocolHandler(const OpString& uri_string, OpString& protocol, OpString& handler)
{
	if (!handler.Reserve(MAX_PATH))
		return OpStatus::ERR_NO_MEMORY;
	if (GetShellProtocolHandler(protocol.CStr(), NULL, 0, handler.CStr(), handler.Capacity()))
	{
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}


OP_STATUS WindowsOpSystemInfo::GetDriveLetters(OpString *result)
{ 
	if (result == NULL)
		return OpStatus::ERR;

	/* We can have a maximum of 26 letters. */
	uni_char s[26], /* ARRAY OK 2005-10-25 peter */
		     *p = s;

	DWORD drives = GetLogicalDrives();
	DWORD bitmask = 1;
	for (uni_char i = 'A'; i <= 'Z'; ++ i)
	{
		if (drives & bitmask)
		{
			*(p ++) = i;
		}
		bitmask <<= 1;
	}

	return result->Set(s, p - s);
}


uni_char WindowsOpSystemInfo::GetCurrentDriveLetter()
{
	return _getdrive() + 'A' - 1;
}


# ifdef PI_CAP_PATHSEQUAL
OP_STATUS WindowsOpSystemInfo::PathsEqual(const uni_char* p1, const uni_char* p2, BOOL* equal)
{
	TRAPD(rc, *equal=PathsEqualL(p1, p2));
	return rc;
}
# endif // PI_CAP_PATHSEQUAL

# if defined(OPSYSTEMINFO_PATHSEQUAL) || defined(PI_CAP_PATHSEQUAL)
BOOL WindowsOpSystemInfo::PathsEqualL(const uni_char* p1, const uni_char* p2)
{
	if(!p1 || !p2)
	{
		// fixes http://crashlog/show_stacktrace?id=5305729 and lots of dupes
		return FALSE;
	}
	uni_char p1_full[MAX_PATH];
	if (!PathSearchAndQualify(p1, p1_full, MAX_PATH))
		return FALSE;
	uni_char p2_full[MAX_PATH];
	if (!PathSearchAndQualify(p2, p2_full, MAX_PATH))
		return FALSE;

	return uni_strnicmp(p1_full, p2_full, MAX_PATH) == 0;
}
#endif //OPSYSTEMINFO_PATHSEQUAL || PI_CAP_PATHSEQUAL


OP_STATUS WindowsOpSystemInfo::OpenFileFolder(const OpStringC & file_path, BOOL treat_folders_as_files)
{
	OpString exe_path;
	OpString params;
	if (!PathFileExists(file_path))
		return OpStatus::ERR;

	OP_STATUS s = GetExePath(exe_path);
	if(OpStatus::IsSuccess(s))
	{
		int pos = exe_path.FindLastOf(UNI_L(PATHSEPCHAR));
		if(pos != KNotFound)
			exe_path[pos] = 0;	//strip the exe name so we just have the path

		// Because of DSK-317420, we need to ensure current directory points to the opera.exe install path
		uni_char* current_dir = NULL;

		current_dir = (uni_char*)g_memory_manager->GetTempBuf2k();
		DWORD curdir_len = GetCurrentDirectory(UNICODE_DOWNSIZE(g_memory_manager->GetTempBuf2kLen()), current_dir);

		// Change to the opera.exe directory 
		if (!SetCurrentDirectory(exe_path.CStr()))
			return OpStatus::ERR;

		if (PathIsDirectory(file_path) && !treat_folders_as_files)
		{
			params.AppendFormat(UNI_L("\"%s\""), file_path.CStr());
		}
		else
		{
			params.AppendFormat(UNI_L("/select,\"%s\""), file_path.CStr());
		}
		HINSTANCE result = ShellExecute(NULL, NULL, UNI_L("explorer.exe"), params.CStr() , NULL, SW_SHOW);
		if (result > (HINSTANCE)32) 
			s = OpStatus::OK;
		else if (result == 0)
			s = OpStatus::ERR_NO_MEMORY;
		else
			s = OpStatus::ERR;

		// Change back to the previous working directory, if we successfully retrieved one previously
		if (curdir_len)
			SetCurrentDirectory(current_dir);
	}
	return s;
}

BOOL WindowsOpSystemInfo::IsFullKeyboardAccessActive()
{
	return TRUE;
}

UINT32 WindowsOpSystemInfo::GetCaretWidth()
{
	DWORD width;
	return SystemParametersInfo(SPI_GETCARETWIDTH, 0, &width, 0)?width:1;
}


HFONT WindowsOpSystemInfo::GetMenuFont()
{
	if (!m_menu_font)
	{
		NONCLIENTMETRICS ncm;
		ncm.cbSize = sizeof(NONCLIENTMETRICS);
		SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0);

		m_menu_font = CreateFontIndirect(&ncm.lfMenuFont);
	}

	return m_menu_font;
}

BOOL WindowsOpSystemInfo::GetIsExecutable(OpString* filename)
{
	OpString extension;
	extension.Set( ((OpString*)filename)->SubString(filename->FindLastOf('.')+1) );

	return (
		    !extension.CompareI(UNI_L("EXE")) || 
		    !extension.CompareI(UNI_L("COM")) ||
			!extension.CompareI(UNI_L("BAT")) ||
			!extension.CompareI(UNI_L("PIF")) ||
			!extension.CompareI(UNI_L("CMD")) ||
			!extension.CompareI(UNI_L("VBS")) ||
			!extension.CompareI(UNI_L("SCR")) ||
			!extension.CompareI(UNI_L("MSI")) ||
			!extension.CompareI(UNI_L("REG")) ||
			!extension.CompareI(UNI_L("LNK")) ||
			!extension.CompareI(UNI_L("WSH")) ||
			!extension.CompareI(UNI_L("MSH")) ||
			!extension.CompareI(UNI_L("WMF"))	);

}

OP_STATUS WindowsOpSystemInfo::GetIllegalFilenameCharacters(OpString* illegalchars)
{
	return illegalchars->Set(UNI_L("<>:\"/\\?|*"));
}

OP_STATUS WindowsOpSystemInfo::GetNewlineString(OpString* newline_string)
{
	return newline_string->Set(UNI_L("\r\n"));
}

INT32 WindowsOpSystemInfo::GetShiftKeyState()
{
	INT32 shift_keys = SHIFTKEY_NONE;

	if(GetKeyState(VK_SHIFT) & 0x8000)
		shift_keys |= SHIFTKEY_SHIFT;

	if(GetKeyState(VK_MENU) & 0x8000)
		shift_keys |= SHIFTKEY_ALT;

	if(GetKeyState(VK_CONTROL) & 0x8000)
		shift_keys |= SHIFTKEY_CTRL;

	return shift_keys;
}

OP_STATUS WindowsOpSystemInfo::ExpandSystemVariablesInString(const uni_char* in, OpString* out)
{
	//julienp:	This function is correct. We are using long names for serialized paths because of a mixup
	//			that happened with the short versions. Do not touch this function without talking to me or
	//			adamm first.

	if(out == NULL)
	{
		return OpStatus::ERR_NULL_POINTER;
	}

	out->Delete(0);
	RETURN_OOM_IF_NULL(out->Reserve(MAX_PATH));
	DWORD retlen = ExpandEnvironmentStrings(in, out->CStr(), out->Capacity());	

	if (retlen == 0)
		return OpStatus::ERR;

	if ((*out)[0] == '{')
	{
		int closing_brace = out->FindFirstOf('}');
		if (closing_brace == KNotFound)
			return OpStatus::ERR;

		uni_char* c = out->CStr() + closing_brace;

		while (*(++c))
			if(*c == '/')
				*c ='\\';

		OpString folder_path;
		WindowsOpDesktopResources resources;
		OpString home_folder;
		RETURN_IF_ERROR(resources.GetHomeFolder(home_folder));
		if (home_folder[home_folder.Length() - 1] != '\\')
			RETURN_IF_ERROR(home_folder.Append(UNI_L("\\")));

		if (out->Compare(UNI_L("{LargePrefs}"), 12) == 0)  // Opera, before version 10.10 used a shorter version of this serialized path 
		{
			out->Delete(0, 12);
			RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_HOME_FOLDER, folder_path));
			RETURN_IF_ERROR(out->Insert(0, folder_path));
		}
		else if (out->Compare(UNI_L("{LargePreferences}"), 18) == 0)
		{
			out->Delete(0, 18);
			RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_LOCAL_HOME_FOLDER, folder_path));
			RETURN_IF_ERROR(out->Insert(0, folder_path));
		}
		else if (out->Compare(UNI_L("{SmallPrefs}"), 12) == 0)  // Opera, before version 10.10 used a shorter version of this serialized path 
		{
			out->Delete(0, 12);
			RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_LOCAL_HOME_FOLDER, folder_path));
			RETURN_IF_ERROR(out->Insert(0, folder_path));
		}
		else if (out->Compare(UNI_L("{SmallPreferences}"), 18) == 0)
		{
			out->Delete(0, 18);
			RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_HOME_FOLDER, folder_path));
			RETURN_IF_ERROR(out->Insert(0, folder_path));
		}
		else if (out->Compare(UNI_L("{Preferences}"), 13) == 0) //Unix profiles use this
		{
			out->Delete(0, 13);
			RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_HOME_FOLDER, folder_path));
			RETURN_IF_ERROR(out->Insert(0, folder_path));
		}
		else if (out->Compare(UNI_L("{Users}"), 7) == 0) //mac profiles from opera 11.0x and earlier use this
		{
			out->Delete(0, 7);
			RETURN_IF_ERROR(out->Insert(0, home_folder));
		}
		else if (out->Compare(UNI_L("{Home}"), 6) == 0)
		{
			out->Delete(0, 6);
			RETURN_IF_ERROR(out->Insert(0, home_folder));
		}
		else if (out->Compare(UNI_L("{Resources}"), 11) == 0)
		{
			out->Delete(0, 11);
			RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_RESOURCES_FOLDER, folder_path));
			RETURN_IF_ERROR(out->Insert(0, folder_path));
		}
		else if (out->Compare(UNI_L("{Binaries}"), 10) == 0)
		{
			out->Delete(0, 10);
			RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_RESOURCES_FOLDER, folder_path));
			RETURN_IF_ERROR(out->Insert(0, folder_path));
		}
		else
			return OpStatus::ERR;
	}

	return OpStatus::OK;
}

BOOL WindowsOpSystemInfo::RemoveIllegalFilenameCharacters(OpString& path, BOOL replace)
{
	return RemoveIllegalCharactersFromPath(path, replace, TRUE);
}

BOOL WindowsOpSystemInfo::RemoveIllegalPathCharacters(OpString& path, BOOL replace)
{
	return RemoveIllegalCharactersFromPath(path, replace, FALSE);
}

BOOL WindowsOpSystemInfo::RemoveIllegalCharactersFromPath(OpString& path, BOOL replace, BOOL file)
{
	path.Strip();
	int i;
	BOOL drive_colon = FALSE;

	OpString unwanted;
	if (file)
		unwanted.Set(UNI_L("/:*?<>|\\\""));
	else
		unwanted.Set(UNI_L("/:*?<>|\""));

	while ((i = path.FindFirstOf(unwanted)) != KNotFound)
	{
		if (!file && i == 1 && ':' == path[i])	// colon is allowed as 2nd char in MSWIN path
		{
			drive_colon= TRUE;
			path[i] = '_';		//replacing here, so it doesn't kick in again
		}
		else
		{
			if( replace )
			{
				path[i] = '_';
			}
			else
			{
				path.Delete(i, 1);
			}
		}
	}

	if (drive_colon)
		path[1] = ':';

	return path.HasContent();
}


INT32 WindowsOpSystemInfo::GetCurrentProcessId()
{
	return (INT32)::GetCurrentProcessId();	
}

#ifndef NO_EXTERNAL_APPLICATIONS
#ifdef DU_REMOTE_URL_HANDLER
OP_STATUS WindowsOpSystemInfo::PlatformExecuteApplication(const uni_char* appin, const uni_char* filenamein, BOOL silent_errors)
#else
OP_STATUS WindowsOpSystemInfo::ExecuteApplication(const uni_char* appin, const uni_char* filenamein, BOOL silent_errors)
#endif
{

	OpString application;
	OpString parameters;
	OP_STATUS err = ProcessAppParameters(appin, filenamein, application, parameters);

	if (OpStatus::IsError(err))
	{
		if (!silent_errors)
			SimpleDialog::ShowDialog(WINDOW_NAME_MESSAGE_DIALOG, NULL, Str::SI_ERR_SHELLEXEC_UNDEFINED, Str::S_OPERA, Dialog::TYPE_OK, Dialog::IMAGE_ERROR);
		return err;
	}

	HINSTANCE error;
	if(filenamein != NULL && uni_stristr(filenamein, UNI_L("-widget")) != NULL )
	{
		error = ShellExecute(g_main_hwnd, NULL, application.CStr(), filenamein, NULL, SW_NORMAL); 	
	}
	else
	{
		error = ShellExecute(g_main_hwnd, NULL, application.CStr(), parameters.CStr(), NULL, SW_NORMAL); 
	}


	if (error <= (HINSTANCE)32)
	{
		OpString msg_top, msg_full, title;

		switch ((ULONG_PTR)error)
		{
			case 0:
				if (OpStatus::IsSuccess(g_languageManager->GetString(Str::SI_ERR_WINEXEC_OUT_OF_SYS_MEMORY, msg_top)))
					OpStatus::Ignore(msg_full.AppendFormat(UNI_L("%s\r\n\r\n%s"), msg_top.CStr(), appin));
				break;
			case ERROR_FILE_NOT_FOUND:
				if (OpStatus::IsSuccess(g_languageManager->GetString(Str::SI_ERR_WINEXEC_FILE_NOT_FOUND, msg_top)))
					OpStatus::Ignore(msg_full.AppendFormat(UNI_L("%s\r\n\r\n%s"), msg_top.CStr(), appin));
				break;
			case ERROR_PATH_NOT_FOUND:
				if (OpStatus::IsSuccess(g_languageManager->GetString(Str::SI_ERR_WINEXEC_PATH_NOT_FOUND, msg_top)))
					OpStatus::Ignore(msg_full.AppendFormat(UNI_L("%s\r\n\r\n%s"), msg_top.CStr(), appin));
				break;
			case ERROR_NOT_ENOUGH_MEMORY:
				if (OpStatus::IsSuccess(g_languageManager->GetString(Str::SI_ERR_WINEXEC_OUT_OF_MEMORY, msg_top)))
					OpStatus::Ignore(msg_full.AppendFormat(UNI_L("%s\r\n\r\n%s"), msg_top.CStr(), appin));
				break;
			case ERROR_BAD_UNIT:
				if (OpStatus::IsSuccess(g_languageManager->GetString(Str::SI_ERR_WINEXEC_DLL_INVALID, msg_top)))
					OpStatus::Ignore(msg_full.AppendFormat(UNI_L("%s\r\n\r\n%s"), msg_top.CStr(), appin));
				break;
			case ERROR_NOT_READY:
				if (OpStatus::IsSuccess(g_languageManager->GetString(Str::SI_ERR_WINEXEC_REQUIRES_32BIT, msg_top)))
					OpStatus::Ignore(msg_full.AppendFormat(UNI_L("%s\r\n\r\n%s"), msg_top.CStr(), appin));
				break;
			case 31:
				OpStatus::Ignore(g_languageManager->GetString(Str::SI_ERR_SHELLEXEC_UNDEFINED, msg_full));
				break;
			default:
				if (OpStatus::IsSuccess(g_languageManager->GetString(Str::SI_ERR_WINEXEC_COULD_NOT_RUN, msg_top)))
					OpStatus::Ignore(msg_full.AppendFormat(UNI_L("%s\r\n\r\n%s"), msg_top.CStr(), appin));
				break;
		}

		OpStatus::Ignore(g_languageManager->GetString(Str::S_OPERA, title));
		if (title.HasContent() && msg_full.HasContent())
			OpStatus::Ignore(SimpleDialog::ShowDialog(WINDOW_NAME_MESSAGE_DIALOG, NULL, msg_full.CStr(), title.CStr(), Dialog::TYPE_OK, Dialog::IMAGE_ERROR));

		return OpStatus::ERR;
	}

	return OpStatus::OK;
}

#ifdef EXTERNAL_APPLICATIONS_SUPPORT 
OP_STATUS WindowsOpSystemInfo::OpenURLInExternalApp(const URL& url) 
{
	int result = reinterpret_cast<int>(ShellExecute(g_main_hwnd, NULL, url.UniName(PASSWORD_SHOW), NULL, UNI_L(""), SW_SHOWNORMAL));
	return (result >= 32) ? OpStatus::OK : OpStatus::ERR;
}
#endif // EXTERNAL_APPLICATIONS_SUPPORT


BOOL WindowsOpSystemInfo::AppIsShellExecutable(const uni_char* app)
{
		//Check if the application is available in the path
		if (SearchPath(NULL, app, L".exe", 0, NULL, NULL))
			return TRUE;

		//If not, check if there is a key telling the command to run the application
		//with this name from the registry. ShellExecute seems to run those as well.
		if (uni_strchr(app, '\\') != 0)
			return FALSE;

		OpString regkey;
		if (uni_strchr(app, '.') == 0)
			regkey.AppendFormat(UNI_L("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\%s.exe"), app);
		else
			regkey.AppendFormat(UNI_L("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\%s"), app);

		HKEY key;
		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, regkey.CStr(), 0, KEY_READ, &key) != ERROR_SUCCESS)
			return FALSE;

		RegCloseKey(key);

		return TRUE;
}

OP_STATUS WindowsOpSystemInfo::SplitOpenCommand(const uni_char* command, OpString& application, OpString& parameters)
{
	OP_ASSERT(command);

	const uni_char* params_start = NULL;
	const uni_char* app_start_ptr = command;
	const uni_char* app_end_ptr = NULL;

	// easy case, when application part is quoted
	// example : "C:\Program Files\iTunes\iTunes.exe" /url "%1"
	if (command[0] == UNI_L('"'))
	{
		const uni_char* matching_quote_ptr = uni_strchr(command + 1, UNI_L('"'));
		// no matching quote
		if (matching_quote_ptr == NULL)
			return OpStatus::ERR_PARSING_FAILED;
		app_start_ptr = command + 1;
		app_end_ptr = matching_quote_ptr;
		params_start = matching_quote_ptr + 1;

		// if parameters exist, there must be a space between the quote and parameters
		if (*params_start && (*params_start != UNI_L(' ')))
			return OpStatus::ERR_PARSING_FAILED;
	}
	else
	{
		// normal case like : C:\Windows\iTunes.exe /url "%1"
		// split on the first space
		const uni_char* space_ptr = uni_strchr(command, UNI_L(' '));
		if (space_ptr != NULL)
		{
			app_end_ptr = space_ptr;
			params_start = space_ptr + 1;
		}
	}

	if (app_end_ptr)
		RETURN_IF_ERROR(application.Set(app_start_ptr, app_end_ptr - app_start_ptr));
	else
		RETURN_IF_ERROR(application.Set(app_start_ptr));

	// application can't be an empty string
	if (application.IsEmpty())
		return OpStatus::ERR_PARSING_FAILED;

	if (!params_start)
		parameters.Empty();
	else 
	{
		// Skip over all the whitespace in front of parameters. The parameters string start after that.
		while (*params_start == UNI_L(' '))
			params_start++;
		RETURN_IF_ERROR(parameters.Set(params_start));
	}

	return OpStatus::OK;
}

OP_STATUS WindowsOpSystemInfo::ProcessAppParameters(const uni_char* appin, const uni_char* filenamein, OpString& application, OpString& parameters)
{
	if (!appin || !*appin)
	{
		return OpStatus::ERR;
	}

	// Sometimes, the application and the filename passed are the same, but only the filename has the path.
	// In this case, only run the filename.
	// example : appin = nodepad.exe, filenamein = c:\Windows\notepad.exe
	const uni_char* file_without_path = filenamein ? uni_strrchr(filenamein, PATHSEPCHAR) : NULL;
	if (file_without_path && (uni_stricmp(appin, file_without_path + 1) == 0)
		&& AppIsShellExecutable(filenamein))
	{
		RETURN_IF_ERROR(application.Set(filenamein));
		parameters.Empty();
		return OpStatus::OK;
	}

	// If the full command denotes an executable, don't split it, take as a whole.
	if (AppIsShellExecutable(appin))
	{
		RETURN_IF_ERROR(application.Set(appin));
		parameters.Empty();
	}
	else
	{
		RETURN_IF_ERROR(SplitOpenCommand(appin, application, parameters));
		if (!AppIsShellExecutable(application.CStr()))
			return OpStatus::ERR;
	}

	// set the file name
	// remove the quotes if it's quoted
	OpString file_name;
	if (filenamein)
	{
		int len = uni_strlen(filenamein);

		if ((filenamein[len - 1] == UNI_L('"')) && (*filenamein == UNI_L('"')))
			file_name.Set(filenamein + 1, len - 2);
		else
			file_name.Set(filenamein);
	}

	// No filename to take into account. We are done
	if (file_name.IsEmpty())  
		return OpStatus::OK;

	// No parameters string. The filename is the only parameter
	if (parameters.IsEmpty())  
		RETURN_IF_ERROR(parameters.Set(UNI_L("\"%1\"")));

	// replace "%1","%l" and "%L" with "%s", why these three "%*" ? see the post below if it's correct
	// http://social.msdn.microsoft.com/Forums/en-US/windowsgeneraldevelopmentissues/thread/71e66c5c-57e9-4fe3-abce-2e0071a2c97d/
	int replaced = parameters.Find(UNI_L("%1")); 
	if(replaced != KNotFound)
		parameters[replaced+1] = UNI_L('s');
	replaced = parameters.Find(UNI_L("%l")); 
	if(replaced != KNotFound)
		parameters[replaced+1] = UNI_L('s');
	replaced = parameters.Find(UNI_L("%L")); 
	if(replaced != KNotFound)
		parameters[replaced+1] = UNI_L('s');

	replaced = parameters.Find(UNI_L("%s"));
	if (replaced == KNotFound)	//There is no %s in the string, we append the parameter at the end
	{
		return parameters.AppendFormat(UNI_L(" \"%s\""), file_name.CStr());
	}

	if (replaced == 0 || parameters[replaced-1] != UNI_L('"'))
	{
		if (parameters[replaced+2] == UNI_L('"'))			//No quote before but a quote after
			return OpStatus::ERR;
	}
	else if(parameters[replaced+2] != UNI_L('"'))			//A quote before but no quote after
		return OpStatus::ERR;

	if (parameters[replaced+2] != UNI_L('\"'))
	{
		file_name.Insert(0, UNI_L("\""));
		file_name.Append(UNI_L("\""));
	}

	OpString parameters2;
	parameters2.AppendFormat(parameters.CStr(),file_name.CStr());
	parameters.Set(parameters2);

	return OpStatus::OK;
}

#endif // !NO_EXTERNAL_APPLICATIONS

OP_STATUS
WindowsOpSystemInfo::GetSystemIp(OpString& ip)
{
	WORD wVersionRequested;
	WSADATA wsaData;
	char name[255];
	OP_STATUS result = OpStatus::OK;
	
	PHOSTENT hostinfo;
	wVersionRequested = MAKEWORD( 1, 1 );
	if ( WSAStartup( wVersionRequested, &wsaData ) == 0 )
	{
		if( gethostname ( name, sizeof(name)) == 0)
		{
			if((hostinfo = gethostbyname(name)) != NULL)
			{
				const char* localIP = inet_ntoa (*(struct in_addr *)*hostinfo->h_addr_list);
				if (localIP)
				{
					result = ip.Set(localIP);
				}
			}
		}
   		WSACleanup( );
	}

	if (OpStatus::IsError(result))
	{
		return result;
	}

	if (ip.IsEmpty())
	{
		return ip.Set("127.0.0.1");
	}

	return OpStatus::OK;
}

const char *WindowsOpSystemInfo::GetOSStr(UA_BaseStringId)
{
	if (!m_os_ver[0])
	{
		GetWinVersionStr(m_os_ver);
	}
	return m_os_ver;
}

const uni_char *WindowsOpSystemInfo::GetPlatformStr()
{
#ifdef _WIN64
	return UNI_L("Win64");
#else
	return UNI_L("Win32");
#endif // _WIN64
}

DesktopOpSystemInfo::DefaultBrowser WindowsOpSystemInfo::GetDefaultBrowser()
{
	DefaultBrowser default_browser = DEFAULT_BROWSER_UNKNOWN;

	OpString key_value;
	if (key_value.Reserve(MAX_PATH) != 0)
	{
		// See if anything familiar is the default for the http protocol.
		if (OpRegReadStrValue(HKEY_CLASSES_ROOT, UNI_L("HTTP\\shell\\open\\command"),
			0, key_value.CStr(), key_value.Capacity()) == ERROR_SUCCESS)
		{
			if (key_value.FindI("firefox.exe") != KNotFound)
				default_browser = DEFAULT_BROWSER_FIREFOX;
			else if (key_value.FindI("iexplore.exe") != KNotFound)
				default_browser = DEFAULT_BROWSER_IE;
			else if (key_value.FindI("opera.exe") != KNotFound)
				default_browser = DEFAULT_BROWSER_OPERA;
		}
	}

	return default_browser;
}


OP_STATUS WindowsOpSystemInfo::GetDefaultBrowserBookmarkLocation(
	DesktopOpSystemInfo::DefaultBrowser default_browser, OpString& location)
{
	if (default_browser == DEFAULT_BROWSER_UNKNOWN)
		return OpStatus::ERR;

	switch (default_browser)
	{
		case DEFAULT_BROWSER_FIREFOX :
		{
			RETURN_IF_ERROR(GetFirefoxBookmarkLocation(location));
			break;
		}
		case DEFAULT_BROWSER_IE :
		{
			RETURN_IF_ERROR(GetIEBookmarkLocation(location));
			break;
		}
		case DEFAULT_BROWSER_OPERA :
		{
			RETURN_IF_ERROR(GetOperaBookmarkLocation(location));
			break;
		}
		default:
		{
			return OpStatus::ERR_NOT_SUPPORTED;
		}
	}

	return OpStatus::OK;
}


OP_STATUS WindowsOpSystemInfo::GetFirefoxBookmarkLocation(OpString& location) const
{
	RETURN_IF_ERROR(WindowsOpDesktopResources::GetFolderPath(FOLDERID_RoamingAppData, CSIDL_APPDATA, location));
	RETURN_IF_ERROR(location.Append("\\Mozilla\\Firefox\\"));

	// Fetch the first profile path from profiles.ini, and assume it's the default one.
	PrefsFile ini_reader(PREFS_STD);

	TRAPD(status, ini_reader.ConstructL());
	RETURN_IF_ERROR(status);

	{
		OpString profiles_ini_path;
		RETURN_IF_ERROR(profiles_ini_path.Set(location));
		RETURN_IF_ERROR(profiles_ini_path.Append("profiles.ini"));

		OpFile ini_file;
		RETURN_IF_ERROR(ini_file.Construct(profiles_ini_path.CStr()));

		TRAP(status, ini_reader.SetFileL(&ini_file));
		RETURN_IF_ERROR(status);
	}

	RETURN_IF_ERROR(ini_reader.LoadAllL());

	// Thankfully, the first profile is always named Profile0.
	OpString path;
	TRAP(status, ini_reader.ReadStringL(UNI_L("Profile0"), UNI_L("Path"), path));
	RETURN_IF_ERROR(status);

	// The location and the path should now, together, form the directory
	// where the bookmarks file, bookmarks.html, reside.
	RETURN_IF_ERROR(location.Append(path));
	do
	{
		OpString places;
		if (OpStatus::IsError(places.Append(location))) break;
		if (OpStatus::IsError(places.Append("\\places.sqlite"))) break;
		if (HotlistParser::FileIsPlacesSqlite(places))
			return location.Set(places);
	} while (0);
	RETURN_IF_ERROR(location.Append("\\bookmarks.html"));

	return OpStatus::OK;
}


OP_STATUS WindowsOpSystemInfo::GetIEBookmarkLocation(OpString& location) const
{
	if (location.Reserve(MAX_PATH) == 0)
		return OpStatus::ERR_NO_MEMORY;

	if (GetFavoritesFolder(location.CStr(), location.Capacity()) == 0)
		return OpStatus::ERR;

	return OpStatus::OK;
}


OP_STATUS WindowsOpSystemInfo::GetOperaBookmarkLocation(OpString& location) const
{
	// First we need to get the path where Opera is installed.
	OpString key_value;
	if (key_value.Reserve(MAX_PATH) == 0)
		return OpStatus::ERR_NO_MEMORY;

	if (OpRegReadStrValue(HKEY_CLASSES_ROOT, UNI_L("HTTP\\shell\\open\\command"),
		0, key_value.CStr(), key_value.Capacity()) != ERROR_SUCCESS)
	{
		return OpStatus::ERR;
	}

	if (key_value.HasContent() && key_value[0] == '"')
		key_value.Delete(0, 1);
	if (key_value.HasContent() && key_value[key_value.Length() - 1] == '"')
		key_value.Delete(key_value.Length() - 1);

	OpString opera_path;
	SplitFilenameIntoComponentsL(key_value, &opera_path, 0, 0);

	// See if this opera is a multi user install.
	BOOL is_multi_user = 0;

	{
		PrefsFile ini_reader(PREFS_STD);

		TRAPD(status, ini_reader.ConstructL());
		RETURN_IF_ERROR(status);

		{
			OpString def_ini_path;
			RETURN_IF_ERROR(def_ini_path.Set(opera_path));
			RETURN_IF_ERROR(def_ini_path.Append(DESKTOP_RES_OPERA_PREFS_GLOBAL));

			OpFile ini_file;
			RETURN_IF_ERROR(ini_file.Construct(def_ini_path.CStr()));

			TRAP(status, ini_reader.SetFileL(&ini_file));
			RETURN_IF_ERROR(status);
		}

		RETURN_IF_ERROR(ini_reader.LoadAllL());

		is_multi_user = (ini_reader.ReadIntL(UNI_L("System"), UNI_L("Multi User")) != 0);
	}

	// If this is a multi user install, the opera ini file will reside inside
	// [Application Data]/Opera/[Opera-dir] otherwise it'll just be inside
	// the profile folder in the application directory.
	OpString opera_resources_path;
	OpString opera_ini_path;

	if (is_multi_user)
	{
		// Find the path of the application data directory.
		RETURN_IF_ERROR(WindowsOpDesktopResources::GetFolderPath(FOLDERID_RoamingAppData, CSIDL_APPDATA, opera_resources_path));

		// Figure out the name of the directory that Opera is residing in, and
		// append it to the profile path.
		const int pathsep_pos = opera_path.SubString(0, opera_path.Length() - 1).FindLastOf('\\');
		if (pathsep_pos == KNotFound)
			return OpStatus::ERR;

		RETURN_IF_ERROR(opera_resources_path.AppendFormat(UNI_L("\\Opera%s"),
			opera_path.CStr() + pathsep_pos));
	}
	else
	{
		RETURN_IF_ERROR(opera_resources_path.Set(opera_path));
		RETURN_IF_ERROR(opera_resources_path.Append("profile\\"));
	}

	RETURN_IF_ERROR(opera_ini_path.Set(opera_resources_path));
	RETURN_IF_ERROR(opera_ini_path.Append(DESKTOP_RES_OPERA_PREFS));

	// Read the opera ini file to find out where the bookmarks reside.
	PrefsFile ini_reader(PREFS_STD);
	RETURN_IF_LEAVE(ini_reader.ConstructL());

	{
		OpFile ini_file;
		RETURN_IF_ERROR(ini_file.Construct(opera_ini_path.CStr()));

		RETURN_IF_LEAVE(ini_reader.SetFileL(&ini_file));
	}

	RETURN_IF_LEAVE(RETURN_IF_ERROR(ini_reader.LoadAllL()));

	if (ini_reader.IsKey("User Prefs", "Hot List File Ver2"))
		RETURN_IF_LEAVE(ini_reader.ReadStringL("User Prefs", "Hot List File Ver2", location));
	else
	{
		// pref is not set - return default path
		RETURN_IF_ERROR(location.Set(opera_resources_path));
		RETURN_IF_ERROR(location.Append(DESKTOP_RES_PACKAGE_BOOKMARK));
	}

	return OpStatus::OK;
}

OP_STATUS WindowsOpSystemInfo::GetUserLanguages(OpString *result)
{
	// Windows does not have a prioritized language list in its user settings,
	// so just fetch the language set in the control panel and use it.

	wchar_t langcode[8]; /* ARRAY OK 2005-12-15 peter - Codes are max three letters anyway */
	wchar_t ctrycode[8]; /* ARRAY OK 2005-12-15 peter - Codes are max three letters anyway */

	// Try getting the ISO 639 code. This API only exists for Windows 98 and
	// later, and NT 4.0 and later.
	int retval = GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SISO639LANGNAME, langcode, ARRAY_SIZE(langcode));
	if (0 == retval)
	{
		// We are probably running Windows 95. We have no usable API to get the
		// ISO 639 language code. LOCALE_SABBREVLANGNAME is close, but not close
		// enough - http://blogs.msdn.com/michkap/archive/2005/02/17/375235.aspx
		// So we have a lookup table.
		// http://msdn.microsoft.com/library/en-us/intl/nls_238z.asp

		static struct langmap
		{
			LANGID lang_id;
			const char *lang;
			const char *ctry;
		}
		lang_map[] =
		{
			{0x0436, "af", "ZA"}, // Afrikaans
			{0x041c, "sq", "AL"}, // Albanian
			{0x0484, "gsw", "FR"}, // Windows Vista and later: Alsatian (France)
			{0x045e, "am", "ET"}, // Windows Vista and later: Amharic (Ethiopia)
			{0x1401, "ar", "DZ"}, // Arabic (Algeria)
			{0x3c01, "ar", "BH"}, // Arabic (Bahrain)
			{0x0c01, "ar", "EG"}, // Arabic (Egypt)
			{0x0801, "ar", "IQ"}, // Arabic (Iraq)
			{0x2c01, "ar", "JO"}, // Arabic (Jordan)
			{0x3401, "ar", "KW"}, // Arabic (Kuwait)
			{0x3001, "ar", "LB"}, // Arabic (Lebanon)
			{0x1001, "ar", "LY"}, // Arabic (Libya)
			{0x1801, "ar", "MA"}, // Arabic (Morocco)
			{0x2001, "ar", "OM"}, // Arabic (Oman)
			{0x4001, "ar", "QA"}, // Arabic (Qatar)
			{0x0401, "ar", "SA"}, // Arabic (Saudi Arabia)
			{0x2801, "ar", "SY"}, // Arabic (Syria)
			{0x1c01, "ar", "TN"}, // Arabic (Tunisia)
			{0x3801, "ar", "AE"}, // Arabic (U.A.E.)
			{0x2401, "ar", "YE"}, // Arabic (Yemen)
			{0x042b, "hy", "AM"}, // Windows 2000 and later: Armenian (Armenia)
			{0x044d, "as", "IN"}, // Windows Vista and later: Assamese (India)
			{0x082c, "az", "AZ"}, // Azeri (Cyrillic)
			{0x042c, "az", "AZ"}, // Azeri (Latin)
			{0x046d, "ba", "RU"}, // Windows Vista and later: Bashkir (Russia)
			{0x042d, "eu", "ES"}, // Basque
			{0x0423, "be", "BY"}, // Belarusian
			{0x0445, "bn", "IN"}, // Windows XP SP2 and later: Bengali (India)
			{0x201a, "bs", "BA"}, // Windows XP SP2 and later (downloadable); Windows Vista and later: Bosnian (Bosnia and Herzegovina - Cyrillic)
			{0x141a, "bs", "BA"}, // Windows XP SP2 and later: Bosnian (Bosnia and Herzegovina - Latin)
			{0x047e, "br", "FR"}, // Breton (France)
			{0x0402, "bg", "BG"}, // Bulgarian (Bulgaria)
			{0x0403, "ca", "ES"}, // Catalan
			{0x0c04, "zh", "HK"}, // Chinese (Hong Kong SAR, PRC)
			{0x1404, "zh", "MO"}, // Windows 98/Me, Windows XP and later: Chinese (Macao SAR)
			{0x0804, "zh", "CN"}, // Chinese (PRC)
			{0x0004, "zh", "Hans"}, // Chinese (Simplified)
			{0x1004, "zh", "SG"}, // Chinese (Singapore)
			{0x0404, "zh", "TW"}, // Chinese (Taiwan)
			{0x7c04, "zh", "Hant"}, // Chinese (Traditional)
			{0x101a, "hr", "BA"}, // Windows XP SP2 and later: Croatian (Bosnia and Herzegovina, Latin)
			{0x041a, "hr", "HR"}, // Croatian (Croatia)
			{0x0405, "cs", "CZ"}, // Czech (Czech Republic)
			{0x0406, "da", "DK"}, // Danish (Denmark)
			{0x048c, "gbz", "AF"}, // Windows XP and later: Dari (Afghanistan)
			{0x0465, "dv", "MV"}, // Windows XP and later: Divehi (Maldives); Unicode only
			{0x0813, "nl", "BE"}, // Dutch (Belgium)
			{0x0413, "nl", "NL"}, // Dutch (Netherlands)
			{0x0c09, "en", "AU"}, // English (Australia)
			{0x2809, "en", "BZ"}, // English (Belize)
			{0x1009, "en", "CA"}, // English (Canada)
			{0x2409, "en", "US"/*"029"*/}, // English (Caribbean)
			{0x4009, "en", "IN"}, // Windows Vista and later: English (India)
			{0x1809, "en", "IE"}, // English (Ireland)
			{0x2009, "en", "JM"}, // English (Jamaica)
			{0x4409, "en", "MY"}, // Windows Vista and later: English (Malaysia)
			{0x1409, "en", "NZ"}, // English (New Zealand)
			{0x3409, "en", "PH"}, // Windows 98/Me, Windows 2000 and later: English (Philippines)
			{0x4809, "en", "SG"}, // Windows Vista and later: English (Singapore)
			{0x1c09, "en", "ZA"}, // English (South Africa)
			{0x2c09, "en", "TT"}, // English (Trinidad)
			{0x0809, "en", "GB"}, // English (United Kingdom)
			{0x0409, "en", "US"}, // English (United States)
			{0x3009, "en", "ZW"}, // Windows 98/Me, Windows 2000 and later: English (Zimbabwe)
			{0x0425, "et", "EE"}, // Estonian (Estonia)
			{0x0438, "fo", "FO"}, // Faeroese (Faero Islands)
			{0x464, "fil", "PH"}, // Windows XP SP2 and later (downloadable); Windows Vista and later: Filipino (Philippines)
			{0x040b, "fi", "FI"}, // Finnish
			{0x040c, "fr", "FR"}, // French (France)
			{0x080c, "fr", "BE"}, // French (Belgium)
			{0x0c0c, "fr", "CA"}, // French (Canada)
			{0x140c, "fr", "LU"}, // French (Luxembourg)
			{0x180c, "fr", "MC"}, // French (Monaco)
			{0x100c, "fr", "CH"}, // French (Switzerland)
			{0x0462, "fy", "NL"}, // Windows XP SP2 and later (downloadable); Windows Vista and later: Frisian (Netherlands)
			{0x0456, "gl", "ES"}, // Windows XP and later: Galician
			{0x0437, "ka", "GE"}, // Windows 2000 and later: Georgian; Unicode only
			{0x0c07, "de", "AT"}, // German (Austria)
			{0x0407, "de", "DE"}, // German (Germany)
			{0x1407, "de", "LI"}, // German (Liechtenstein)
			{0x1007, "de", "LU"}, // German (Luxembourg)
			{0x0807, "de", "CH"}, // German (Switzerland)
			{0x0408, "el", "GR"}, // Greek 
			{0x046f, "kl", "GL"}, // Windows Vista and later: Greenlandic (Greenland)
			{0x0447, "gu", "IN"}, // Windows XP and later: Gujarati
			{0x0468, "ha", "NG"}, // Windows Vista and later: Hausa (Nigeria)
			{0x040d, "he", "IL"}, // Hebrew (Israel)
			{0x0439, "hi", "IN"}, // Windows 2000 and later: Hindi (India)
			{0x040e, "hu", "HU"}, // Hungarian (Hungary)
			{0x040f, "is", "IS"}, // Icelandic (Iceland)
			{0x0421, "id", "ID"}, // Indonesian
			{0x085d, "iu", "CA"}, // Windows XP and later: Inuktitut (Canada - Latin)
			{0x045d, "iu", "CA"}, // Windows XP SP2 and later (downloadable); Windows Vista and later: Inuktitut (Canada - Syllabics)
			{0x083c, "ga", "IE"}, // Windows XP SP2 and later (downloadable); Windows Vista and later: Irish (Ireland)
			{0x0410, "it", "IT"}, // Italian (Italy)
			{0x0810, "it", "CH"}, // Italian (Switzerland)
			{0x0411, "ja", "JP"}, // Japanese (Japan)
			{0x044b, "kn", "IN"}, // Windows XP and later: Kannada
			{0x043f, "kk", "KZ"}, // Windows 2000 and later: Kazakh (Kazakhstan)
			{0x0453, "kh", "KH"}, // Windows Vista and later: Khmer (Cambodia)
			{0x0486, "qut", "GT"}, // Windows Vista and later: K'iche (Guatemala)
			{0x0487, "rw", "RW"}, // Windows Vista and later: Kinyarwanda (Rwanda)
			{0x0457, "kok", "IN"}, // Windows 2000 and later: Konkani
			{0x0412, "ko", "KR"}, // Korean (Korea)
			{0x0440, "ky", "KG"}, // Windows XP and later: Kyrgyz (Kyrgyzstan)
			{0x0454, "lo", "LA"}, // Windows Vista and later: Lao (Lao P.D.R.)
			{0x0426, "lv", "LV"}, // Latvian (Latvia)
			{0x0427, "lt", "LT"}, // Lithuanian (Lithuania)
			{0x082e, "dsb", "DE"}, // Windows Vista and later: Lower Sorbian (Germany)
			{0x046e, "lb", "LU"}, // Windows XP SP2 and later (downloadable); Windows Vista and later: Luxembourgish (Luxembourg)
			{0x042f, "mk", "MK"}, // Windows 2000 and later: Macedonian (FYROM)
			{0x083e, "ms", "BN"}, // Windows 2000 and later: Malay (Brunei Darussalam)
			{0x043e, "ms", "MY"}, // Windows 2000 and later: Malay (Malaysia)
			{0x044c, "ml", "IN"}, // Windows XP SP2 and later: Malayalam (India)
			{0x043a, "mt", "MT"}, // Windows XP SP2 and later: Maltese (Malta)
			{0x0481, "mi", "NZ"}, // Windows XP SP2 and later: Maori (New Zealand)
			{0x047a, "arn", "CL"}, // Windows XP SP2 and later (downloadable); Windows Vista and later: Mapudungun (Chile)
			{0x044e, "mr", "IN"}, // Windows 2000 and later: Marathi (India)
			{0x047c, "moh", "CA"}, // Windows XP SP2 and later (downloadable); Windows Vista and later: Mohawk (Canada)
			{0x0450, "mn", "MN"}, // Windows XP and later: Mongolian (Mongolia)
			{0x0850, "mn", "CN"}, // Windows Vista and later: Mongolian (PRC)
			{0x0461, "ne", "NP"}, // Windows XP SP2 and later (downloadable); Windows Vista and later: Nepali (Nepal)
			{0x0414, "nb", "NO"}, // Norwegian (Bokm�l)
			{0x0814, "nn", "NO"}, // Norwegian (Nynorsk)
			{0x0482, "oc", "FR"}, // Occitan (France)
			{0x0448, "or", "IN"}, // Oriya (India)
			{0x0463, "ps", "AF"}, // Windows XP SP2 and later (downloadable); Windows Vista and later: Pashto (Afghanistan)
			{0x0429, "fa", "IR"}, // Persian (Iran)
			{0x0415, "pl", "PL"}, // Polish (Poland)
			{0x0416, "pt", "BR"}, // Portuguese (Brazil)
			{0x0816, "pt", "PT"}, // Portuguese (Portugal)
			{0x0446, "pa", "IN"}, // Windows XP and later: Punjabi (India)
			{0x046b, "quz", "BO"}, // Windows XP SP2 and later: Quechua (Bolivia)
			{0x086b, "quz", "EC"}, // Windows XP SP2 and later: Quechua (Ecuador)
			{0x0c6b, "quz", "PE"}, // Windows XP SP2 and later: Quechua (Peru)
			{0x0418, "ro", "RO"}, // Romanian (Romania)
			{0x0417, "rm", "CH"}, // Windows XP SP2 and later (downloadable); Windows Vista and later: Romansh
			{0x0419, "ru", "RU"}, // Russian (Russia)
			{0x243b, "smn", "FI"}, // Windows XP SP2 and later: Sami (Inari, Finland)
			{0x103b, "smj", "NO"}, // Windows XP SP2 and later: Sami (Lule, Norway)
			{0x143b, "smj", "SE"}, // Windows XP SP2 and later: Sami (Lule, Sweden)
			{0x0c3b, "se", "FI"}, // Windows XP SP2 and later: Sami (Northern, Finland)
			{0x043b, "se", "NO"}, // Windows XP SP2 and later: Sami (Northern, Norway)
			{0x083b, "se", "SE"}, // Windows XP SP2 and later: Sami (Northern, Sweden)
			{0x203b, "sms", "FI"}, // Windows XP SP2 and later: Sami (Skolt, Finland)
			{0x183b, "sma", "NO"}, // Windows XP SP2 and later: Sami (Southern, Norway)
			{0x1c3b, "sma", "SE"}, // Windows XP SP2 and later: Sami (Southern, Sweden)
			{0x044f, "sa", "IN"}, // Windows 2000 and later: Sanskrit (India)
			{0x1c1a, "sr", "BA"}, // Windows XP SP2 and later: Serbian (Bosnia, and Herzegovina, Cyrillic)
			{0x181a, "sr", "BA"}, // Windows XP SP2 and later: Serbian (Bosnia, and Herzegovina, Latin)
			{0x0c1a, "sr", "CS"}, // Serbian (Serbia and Montenegro, Cyrillic)
			{0x081a, "sr", "CS"}, // Serbian (Serbia and Montenegro, Latin)
			{0x046c, "ns", "ZA"}, // Windows XP SP2 and later: Sesotho sa Leboa/Northern Sotho (South Africa)
			{0x0432, "tn", "ZA"}, // Windows XP SP2 and later: Setswana/Tswana (South Africa)
			{0x045b, "si", "LK"}, // Windows Vista and later: Sinhala (Sri Lanka)
			{0x041b, "sk", "SK"}, // Slovak (Slovakia)
			{0x0424, "sl", "SI"}, // Slovenian (Slovenia)
			{0x2c0a, "es", "AR"}, // Spanish (Argentina)
			{0x400a, "es", "BO"}, // Spanish (Bolivia)
			{0x340a, "es", "CL"}, // Spanish (Chile)
			{0x240a, "es", "CO"}, // Spanish (Colombia)
			{0x140a, "es", "CR"}, // Spanish (Costa Rica)
			{0x1c0a, "es", "DO"}, // Spanish (Dominican Republic)
			{0x300a, "es", "EC"}, // Spanish (Ecuador)
			{0x440a, "es", "SV"}, // Spanish (El Salvador)
			{0x100a, "es", "GT"}, // Spanish (Guatemala)
			{0x480a, "es", "HN"}, // Spanish (Honduras)
			{0x080a, "es", "MX"}, // Spanish (Mexico)
			{0x4c0a, "es", "NI"}, // Spanish (Nicaragua)
			{0x180a, "es", "PA"}, // Spanish (Panama)
			{0x3c0a, "es", "PY"}, // Spanish (Paraguay)
			{0x280a, "es", "PE"}, // Spanish (Peru)
			{0x500a, "es", "PR"}, // Spanish (Puerto Rico)
			{0x0c0a, "es", "ES"}, // Spanish (Spain, modern sort)
			{0x040a, "es", "ES"}, // Spanish (Spain, traditional sort)
			{0x540a, "es", "US"}, // Windows Vista and later: Spanish (United States)
			{0x380a, "es", "UY"}, // Spanish (Uruguay)
			{0x200a, "es", "VE"}, // Spanish (Venezuela)
			{0x0441, "sw", "KE"}, // Windows 2000 and later: Swahili (Kenya)
			{0x081d, "sv", "FI"}, // Swedish (Finland)
			{0x041d, "sv", "SE"}, // Swedish (Sweden)
			{0x045a, "syr", "SY"}, // Windows XP and later: Syriac (Syria)
			{0x0428, "tg", "TJ"}, // Windows Vista and later: Tajik (Tajikistan)
			{0x085f, "tmz", "DZ"}, // Windows Vista and later: Tamazight (Algeria, Latin)
			{0x0449, "ta", "IN"}, // Windows 2000 and later: Tamil (India)
			{0x0444, "tt", "RU"}, // Windows XP and later: Tatar (Russia)
			{0x044a, "te", "IN"}, // Windows XP and later: Telugu (India)
			{0x041e, "th", "TH"}, // Thai (Thailand)
			{0x0851, "bo", "BT"}, // Windows Vista and later: Tibetan (Bhutan)
			{0x0451, "bo", "CN"}, // Windows Vista and later: Tibetan (PRC)
			{0x041f, "tr", "TR"}, // Turkish (Turkey)
			{0x0442, "tk", "TM"}, // Windows Vista and later: Turkmen (Turkmenistan)
			{0x0480, "ug", "CN"}, // Windows Vista and later: Uighur (PRC)
			{0x0422, "uk", "UA"}, // Ukrainian (Ukraine)
			{0x042e, "wen", "DE"}, // Windows Vista and later: Upper Sorbian (Germany)
			{0x0820, "tr", "IN"}, // Urdu (India)
			{0x0420, "ur", "PK"}, // Windows 98/Me, Windows 2000 and later: Urdu (Pakistan)
			{0x0843, "uz", "UZ"}, // Windows 2000 and later: Uzbek (Uzbekistan, Cyrillic)
			{0x0443, "uz", "UZ"}, // Windows 2000 and later: Uzbek (Uzbekistan, Latin)
			{0x042a, "vi", "VN"}, // Windows 98/Me, Windows NT 4.0 and later: Vietnamese
			{0x0452, "cy", "GB"}, // Windows XP SP2 and later: Welsh (United Kingdom)
			{0x0488, "wo", "SN"}, // Windows Vista and later: Wolof (Senegal)
			{0x0434, "xh", "ZA"}, // Windows XP SP2 and later: Xhosa/isiXhosa (South Africa)
			{0x0485, "sah", "RU"}, // Windows Vista and later: Yakut (Russia)
			{0x0478, "ii", "CN"}, // Windows Vista and later: Yi (PRC)
			{0x046a, "yo", "NG"}, // Windows Vista and later: Yoruba (Nigeria)
			{0x0435, "zu", "ZA"}, // Windows XP SP2 and later: Zulu/isiZulu (South Africa)
		};

		LANGID def_lang = GetUserDefaultLangID();
		const char *lang = NULL;
		const char *ctry = NULL;

		for (int i = 0; i < sizeof(lang_map)/sizeof(lang_map[0]); i++)
		{
			langmap *lm = &lang_map[i];
			if (lm->lang_id == def_lang)
			{
				lang = lm->lang;
				ctry = lm->ctry;
				break;
			}
		}

		if (!lang)
		{
			OP_ASSERT(!"Unrecognized locale identifier");
			return result->Set("en");
		}

		make_doublebyte_in_buffer(lang, strlen(lang), langcode, ARRAY_SIZE(langcode));
		make_doublebyte_in_buffer(ctry, strlen(ctry), ctrycode, ARRAY_SIZE(ctrycode));
	}
	else
	{
		// Get the country code
		retval = GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SISO3166CTRYNAME, ctrycode, ARRAY_SIZE(ctrycode));
		if (retval)
		{
			OP_ASSERT(uni_strlen(ctrycode) == 2);
		}
		else if (!retval)
		{
			ctrycode[0] = 0;
		}
	}

	if (ctrycode[0])
	{
		// Bug#214462: We need to define some special cases here
		if (uni_strni_eq_lower(langcode, "nb", 3) ||
		    uni_strni_eq_lower(langcode, "nn", 3))
		{
			// Norwegian bokm�l and nynorsk - several servers only support "no"
			return result->AppendFormat(UNI_L("%s_%s,%s,no_%s,no"), langcode, ctrycode, langcode, ctrycode);
		}
		else if (uni_strni_eq_lower(langcode, "zh", 3))
		{
			// Chinese - add "zh_TW" for traditional and "zh_CN" for
			// simplified.
			// TODO: Perhaps add "zh-Hans" (simp) and "zh-Hant" (trad)?

			if (uni_strni_eq_upper(ctrycode, "HK", 3) ||
			    uni_strni_eq_upper(ctrycode, "HANT", 5))
			{
				return result->AppendFormat(UNI_L("%s_%s,zh_TW,%s"), langcode, ctrycode, langcode);
			}
			else if (uni_strni_eq_upper(ctrycode, "MO", 3) || 
			         uni_strni_eq_upper(ctrycode, "SG", 3) ||
			         uni_strni_eq_upper(ctrycode, "HANS", 5))
			{
				return result->AppendFormat(UNI_L("%s_%s,zh_CN,%s"), langcode, ctrycode, langcode);
			}

			// CN and TW are served through the standard case below:
		}

		return result->AppendFormat(UNI_L("%s_%s,%s"), langcode, ctrycode, langcode);
	}
	else
	{
		// No country specifier, return the language string.
		return result->Set(langcode);
	}
}

OP_STATUS WindowsOpSystemInfo::GetSystemFallbackLanguage(OpString *result)
{
	// This function retrieves the language set in the system locale and
	// maps it to one of the possible language folders in locale.
	// (see translations module)

	static struct langmap
	{
		LANGID lang_id;
		const char *locale_folder;
	}
	lang_map[] =
	{
		{0x0436, "af"}, // Afrikaans
		{0x041c, "sq"}, // Albanian
		{0x045e, "am"}, // Windows Vista and later: Amharic (Ethiopia)
		{0x1401, "ar"}, // Arabic (Algeria)
		{0x3c01, "ar"}, // Arabic (Bahrain)
		{0x0c01, "ar"}, // Arabic (Egypt)
		{0x0801, "ar"}, // Arabic (Iraq)
		{0x2c01, "ar"}, // Arabic (Jordan)
		{0x3401, "ar"}, // Arabic (Kuwait)
		{0x3001, "ar"}, // Arabic (Lebanon)
		{0x1001, "ar"}, // Arabic (Libya)
		{0x1801, "ar"}, // Arabic (Morocco)
		{0x2001, "ar"}, // Arabic (Oman)
		{0x4001, "ar"}, // Arabic (Qatar)
		{0x0401, "ar"}, // Arabic (Saudi Arabia)
		{0x2801, "ar"}, // Arabic (Syria)
		{0x1c01, "ar"}, // Arabic (Tunisia)
		{0x3801, "ar"}, // Arabic (U.A.E.)
		{0x2401, "ar"}, // Arabic (Yemen)
		{0x042b, "hy"}, // Windows 2000 and later: Armenian (Armenia)
		{0x044d, "as"}, // Windows Vista and later: Assamese (India)
		{0x082c, "az"}, // Azeri (Cyrillic)
		{0x042c, "az"}, // Azeri (Latin)
		{0x042d, "eu"}, // Basque
		{0x0423, "be"}, // Belarusian
		{0x0445, "bn"}, // Windows XP SP2 and later: Bengali (India)
		{0x201a, "bs"}, // Windows XP SP2 and later (downloadable); Windows Vista and later: Bosnian (Bosnia and Herzegovina - Cyrillic)
		{0x141a, "bs"}, // Windows XP SP2 and later: Bosnian (Bosnia and Herzegovina - Latin)
		{0x047e, "br"}, // Breton (France)
		{0x0402, "bg"}, // Bulgarian (Bulgaria)
		{0x0403, "ca"}, // Catalan
		{0x0c04, "zh-tw"}, // Chinese (Hong Kong SAR, PRC)
		{0x1404, "zh-cn"}, // Windows 98/Me, Windows XP and later: Chinese (Macao SAR)
		{0x0804, "zh-cn"}, // Chinese (PRC)
		{0x0004, "zh-cn"}, // Chinese (Simplified)
		{0x1004, "zh-cn"}, // Chinese (Singapore)
		{0x0404, "zh-tw"}, // Chinese (Taiwan)
		{0x7c04, "zh-tw"}, // Chinese (Traditional)
		{0x101a, "hr"}, // Windows XP SP2 and later: Croatian (Bosnia and Herzegovina, Latin)
		{0x041a, "hr"}, // Croatian (Croatia)
		{0x0405, "cs"}, // Czech (Czech Republic)
		{0x0406, "da"}, // Danish (Denmark)
		{0x0813, "nl"}, // Dutch (Belgium)
		{0x0413, "nl"}, // Dutch (Netherlands)
		{0x0c09, "en-gb"}, // English (Australia)
		{0x2809, "en"}, // English (Belize)
		{0x1009, "en"}, // English (Canada)
		{0x2409, "en"/*"029"*/}, // English (Caribbean)
		{0x4009, "en-gb"}, // Windows Vista and later: English (India)
		{0x1809, "en-gb"}, // English (Ireland)
		{0x2009, "en-gb"}, // English (Jamaica)
		{0x4409, "en"}, // Windows Vista and later: English (Malaysia)
		{0x1409, "en-gb"}, // English (New Zealand)
		{0x3409, "en"}, // Windows 98/Me, Windows 2000 and later: English (Philippines)
		{0x4809, "en"}, // Windows Vista and later: English (Singapore)
		{0x1c09, "en"}, // English (South Africa)
		{0x2c09, "en"}, // English (Trinidad)
		{0x0809, "en-gb"}, // English (United Kingdom)
		{0x0409, "en"}, // English (United States)
		{0x3009, "en"}, // Windows 98/Me, Windows 2000 and later: English (Zimbabwe)
		{0x0425, "et"}, // Estonian (Estonia)
		{0x040b, "fi"}, // Finnish
		{0x040c, "fr"}, // French (France)
		{0x080c, "fr"}, // French (Belgium)
		{0x0c0c, "fr-ca"}, // French (Canada)
		{0x140c, "fr"}, // French (Luxembourg)
		{0x180c, "fr"}, // French (Monaco)
		{0x100c, "fr"}, // French (Switzerland)
		{0x0462, "fy"}, // Windows XP SP2 and later (downloadable); Windows Vista and later: Frisian (Netherlands)
		{0x0456, "gl"}, // Windows XP and later: Galician
		{0x0437, "ka"}, // Windows 2000 and later: Georgian; Unicode only
		{0x0c07, "de-at"}, // German (Austria)
		{0x0407, "de"}, // German (Germany)
		{0x1407, "de"}, // German (Liechtenstein)
		{0x1007, "de"}, // German (Luxembourg)
		{0x0807, "de"}, // German (Switzerland)
		{0x0408, "el"}, // Greek 
		{0x0447, "gu"}, // Windows XP and later: Gujarati
		{0x0468, "ha"}, // Windows Vista and later: Hausa (Nigeria)
		{0x040d, "he"}, // Hebrew (Israel)
		{0x0439, "hi"}, // Windows 2000 and later: Hindi (India)
		{0x040e, "hu"}, // Hungarian (Hungary)
		{0x040f, "is"}, // Icelandic (Iceland)
		{0x0421, "id"}, // Indonesian
		{0x083c, "ga"}, // Windows XP SP2 and later (downloadable); Windows Vista and later: Irish (Ireland)
		{0x0410, "it"}, // Italian (Italy)
		{0x0810, "it"}, // Italian (Switzerland)
		{0x0411, "ja"}, // Japanese (Japan)
		{0x044b, "kn"}, // Windows XP and later: Kannada
		{0x043f, "kk"}, // Windows 2000 and later: Kazakh (Kazakhstan)
		{0x0412, "ko"}, // Korean (Korea)
		{0x0440, "ky"}, // Windows XP and later: Kyrgyz (Kyrgyzstan)
		{0x0454, "lo"}, // Windows Vista and later: Lao (Lao P.D.R.)
		{0x0426, "lv"}, // Latvian (Latvia)
		{0x0427, "lt"}, // Lithuanian (Lithuania)
		{0x042f, "mk"}, // Windows 2000 and later: Macedonian (FYROM)
		{0x083e, "ms"}, // Windows 2000 and later: Malay (Brunei Darussalam)
		{0x043e, "ms"}, // Windows 2000 and later: Malay (Malaysia)
		{0x044c, "ml"}, // Windows XP SP2 and later: Malayalam (India)
		{0x044e, "mr"}, // Windows 2000 and later: Marathi (India)
		{0x0450, "mn"}, // Windows XP and later: Mongolian (Mongolia)
		{0x0850, "mn"}, // Windows Vista and later: Mongolian (PRC)
		{0x0414, "nb"}, // Norwegian (Bokm�l)
		{0x0814, "nn"}, // Norwegian (Nynorsk)
		{0x0448, "or"}, // Oriya (India)
		{0x0463, "ps"}, // Windows XP SP2 and later (downloadable); Windows Vista and later: Pashto (Afghanistan)
		{0x0429, "fa"}, // Persian (Iran)
		{0x0415, "pl"}, // Polish (Poland)
		{0x0416, "pt-BR"}, // Portuguese (Brazil)
		{0x0816, "pt"}, // Portuguese (Portugal)
		{0x0446, "pa"}, // Windows XP and later: Punjabi (India)
		{0x0418, "ro"}, // Romanian (Romania)
		{0x0419, "ru"}, // Russian (Russia)
		{0x0c3b, "se"}, // Windows XP SP2 and later: Sami (Northern, Finland)
		{0x043b, "se"}, // Windows XP SP2 and later: Sami (Northern, Norway)
		{0x083b, "se"}, // Windows XP SP2 and later: Sami (Northern, Sweden)
		{0x1c1a, "sr"}, // Windows XP SP2 and later: Serbian (Bosnia, and Herzegovina, Cyrillic)
		{0x181a, "sr"}, // Windows XP SP2 and later: Serbian (Bosnia, and Herzegovina, Latin)
		{0x0c1a, "sr"}, // Serbian (Serbia and Montenegro, Cyrillic)
		{0x081a, "sr"}, // Serbian (Serbia and Montenegro, Latin)
		{0x045b, "si"}, // Windows Vista and later: Sinhala (Sri Lanka)
		{0x041b, "sk"}, // Slovak (Slovakia)
		{0x0424, "sl"}, // Slovenian (Slovenia)
		{0x2c0a, "es-LA"}, // Spanish (Argentina)
		{0x400a, "es-LA"}, // Spanish (Bolivia)
		{0x340a, "es-LA"}, // Spanish (Chile)
		{0x240a, "es-CO"}, // Spanish (Colombia)
		{0x140a, "es-LA"}, // Spanish (Costa Rica)
		{0x1c0a, "es-LA"}, // Spanish (Dominican Republic)
		{0x300a, "es-LA"}, // Spanish (Ecuador)
		{0x440a, "es-LA"}, // Spanish (El Salvador)
		{0x100a, "es-LA"}, // Spanish (Guatemala)
		{0x480a, "es-LA"}, // Spanish (Honduras)
		{0x080a, "es-LA"}, // Spanish (Mexico)
		{0x4c0a, "es-LA"}, // Spanish (Nicaragua)
		{0x180a, "es-LA"}, // Spanish (Panama)
		{0x3c0a, "es-LA"}, // Spanish (Paraguay)
		{0x280a, "es-LA"}, // Spanish (Peru)
		{0x500a, "es-LA"}, // Spanish (Puerto Rico)
		{0x0c0a, "es-ES"}, // Spanish (Spain, modern sort)
		{0x040a, "es-ES"}, // Spanish (Spain, traditional sort)
		{0x540a, "es-LA"}, // Windows Vista and later: Spanish (United States)
		{0x380a, "es-LA"}, // Spanish (Uruguay)
		{0x200a, "es-LA"}, // Spanish (Venezuela)
		{0x0441, "sw"}, // Windows 2000 and later: Swahili (Kenya)
		{0x081d, "sv"}, // Swedish (Finland)
		{0x041d, "sv"}, // Swedish (Sweden)
		{0x0428, "tg"}, // Windows Vista and later: Tajik (Tajikistan)
		{0x0449, "ta"}, // Windows 2000 and later: Tamil (India)
		{0x044a, "te"}, // Windows XP and later: Telugu (India)
		{0x041e, "th"}, // Thai (Thailand)
		{0x041f, "tr"}, // Turkish (Turkey)
		{0x0442, "tk"}, // Windows Vista and later: Turkmen (Turkmenistan)
		{0x0422, "uk"}, // Ukrainian (Ukraine)
		{0x0820, "tr"}, // Urdu (India)
		{0x0420, "ur"}, // Windows 98/Me, Windows 2000 and later: Urdu (Pakistan)
		{0x0843, "uz"}, // Windows 2000 and later: Uzbek (Uzbekistan, Cyrillic)
		{0x0443, "uz"}, // Windows 2000 and later: Uzbek (Uzbekistan, Latin)
		{0x042a, "vi"}, // Windows 98/Me, Windows NT 4.0 and later: Vietnamese
		{0x0452, "cy"}, // Windows XP SP2 and later: Welsh (United Kingdom)
		{0x0434, "xh"}, // Windows XP SP2 and later: Xhosa/isiXhosa (South Africa)
		{0x046a, "yo"}, // Windows Vista and later: Yoruba (Nigeria)
		{0x0435, "zu"}, // Windows XP SP2 and later: Zulu/isiZulu (South Africa)
	};

	LANGID def_lang = GetUserDefaultUILanguage();
	for (int i = 0; i < sizeof(lang_map)/sizeof(lang_map[0]); i++)
	{
		langmap *lm = &lang_map[i];
		if (lm->lang_id == def_lang)
		{
			return result->Set(lm->locale_folder);
		}
	}

	return result->Set("en");

}

void WindowsOpSystemInfo::GetUserCountry(uni_char result[3])
{
	if (!GetLocation(result))
	{
		// If unsupported, return according to spec.
		result[0] = 0;
	}
}

OP_STATUS WindowsOpSystemInfo::GenerateGuid(OpGuid &guid)
{
	GUID system_guid;
	unsigned char *ptr;

	HRESULT hr = ::CoCreateGuid(&system_guid);
	if(SUCCEEDED(hr))
	{
		int i;
		int maximum = sizeof(OpGuid);

		// should be the same, 128-bit
		OP_ASSERT(sizeof(OpGuid) == sizeof(GUID));

		ptr = (unsigned char *)&system_guid;
		for(i = 0; i < maximum; i++)
		{
			guid[i] = *(ptr++);
		}
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

//    julienp-271008: The following function should be removed. ComposeExternalMail needs to
//    be rewritten to stop using it.
//
//    DG-1999-12-21 
//    ___________________________________________________________________________ 
//    StrReplace 
// 
//    Copy "pszSource" to "pszDest", replacing every occurrence of 'pszSearchFor' 
//    with 'pszReplaceWith'.  Pointer overlap is OK. 
// 
//    Returns FALSE on error. 
// 
//    To request the needed final length only -- call with 'pszDest'==NULL 
//    ___________________________________________________________________________ 
// 
// 

BOOL StrReplace 
( 
    uni_char*       pszDest,                    //    The result (use NULL to request resulting length only) 
    int             maxDestLen,                 //    Max length of 'pszDest' in characters (NOT including the NULL) 
    const uni_char* pszSource,                  //    Main argument string 
    const uni_char* pszSearchFor,               //    String to search for 
    const uni_char* pszReplaceWith,             //    String to replace the 'pszSearchFor' string 
    int *           pnFinalLenRet = NULL,       //    Opt. out (length of dest) 
    BOOL*           pfDidReplace = NULL         //    Opt. out (TRUE if we did replace some text) 
) 
{ 
    BOOL fCheckLenOnly  = pnFinalLenRet && !pszDest; 
    if (pfDidReplace) 
        *pfDidReplace = FALSE; 
 
    if (! ( 
                IsStr(pszSearchFor) 
            &&  pszReplaceWith 
            &&  ((maxDestLen > 0) || fCheckLenOnly) 
            &&  (pszDest || fCheckLenOnly))) 
        return FALSE; 
 
     
    BOOL            fOk             = FALSE; 
    int             nFinalLen       = 0; 
    int             lenSearchFor    = uni_strlen( pszSearchFor); 
    int             lenReplaceWith  = uni_strlen( pszReplaceWith); 
    int             cbReplaceWith   = sizeof(uni_char)*lenReplaceWith; 
    const uni_char  *pszSearchStart = pszSource; 
    uni_char        *pszFound       = NULL; 
    uni_char        *pszOutStart    = NULL; 
    uni_char        *pszOut         = NULL; 
 
    // 
    //    If buffer overlap we have to use a temp. buffer. 
    //    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    // 
 
    BOOL fBufferOverlap = pszDest && IsPointerOverlapping( pszDest, UNICODE_SIZE(maxDestLen+1), pszSource, UNICODE_SIZE(uni_strlen(pszSource)+1)); 
    if( fBufferOverlap) 
    { 
        //    alloc temp dest buffer 
        pszOutStart = new uni_char[maxDestLen+1]; 
        if( !pszOutStart) 
            goto LABEL_Exit; //OOM7: No separate way to signal memory errors! 
 
        else 
        { 
            pszOut = pszOutStart; 
        } 
    } 
    else 
        pszOut = pszDest; 
 
 
    // 
    //    Search and replace 
    //    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    // 
    do 
    { 
        pszFound = (uni_char*) uni_strstr( pszSearchStart, pszSearchFor); 
        if (pszFound && pfDidReplace) 
            *pfDidReplace = TRUE; 
 
 
        // 
        //    Copy in the characters before the match 
        // 
        int lenHeadSkipped; 
        if (!pszFound) 
        { 
            //    No more matches 
            lenHeadSkipped = uni_strlen( pszSearchStart); 
        } 
        else 
        { 
            lenHeadSkipped = pszFound - pszSearchStart; 
        } 
     
        if (!fCheckLenOnly && (lenHeadSkipped + nFinalLen > maxDestLen)) 
            goto LABEL_Exit; 
        else 
        { 
            if (pszDest) 
            { 
                op_memcpy( pszOut, pszSearchStart, lenHeadSkipped*sizeof(uni_char)); 
                pszOut += lenHeadSkipped; 
            } 
            nFinalLen += lenHeadSkipped; 
        } 
     
 
        // 
        //    copy in the replacement string 
        // 
        if (pszFound) 
        { 
            if (!fCheckLenOnly && (nFinalLen + lenReplaceWith > maxDestLen)) 
                goto LABEL_Exit; 
            else 
            { 
                //    Insert replacement string. 
                if (pszDest) 
                { 
                    op_memcpy( pszOut, pszReplaceWith, cbReplaceWith); 
                    pszOut += lenReplaceWith; 
                } 
                nFinalLen += lenReplaceWith; 
                pszSearchStart = pszFound + lenSearchFor; 
            } 
        } 
        else 
        { 
            // 
            //    Done 
            // 
            fOk = TRUE; 
     
            if (pszDest) 
            { 
                //    if we used a temp buffer (ptr. overlap) then copy in the result from the temp buffer. 
                if (pszOutStart) 
                    op_memcpy( pszDest, pszOutStart, nFinalLen*sizeof(uni_char)); 
 
                //    make sure it's null terminated 
                *(pszDest+nFinalLen) = 0; 
            } 
        } 
    } while (pszFound); 
 
 
    // 
    //    Clean up 
    // 
 
LABEL_Exit: 
    if (pnFinalLenRet) 
        *pnFinalLenRet = nFinalLen; 
    delete[] pszOutStart; 
    return fOk; 
} 

void WindowsOpSystemInfo::ComposeExternalMail(const uni_char* to, const uni_char* cc, const uni_char* bcc, const uni_char* subject, const uni_char* message, const uni_char* raw_address, MAILHANDLER mailhandler)
{
	if (mailhandler == _MAILHANDLER_FIRST_ENUM)
		mailhandler = MAILHANDLER(g_pcui->GetIntegerPref(PrefsCollectionUI::MailHandler));

	switch (mailhandler)
	{		
		case MAILHANDLER_APPLICATION:
		{
			//	%t = to
			//	%c = cc
			//	%b = bcc
			//	%s = subject
			//	%m = message
			OpStringC extApp = g_pcapp->GetStringPref(PrefsCollectionApp::ExternalMailClient);
			BOOL fExtAppSpaceSubst = g_pcapp->GetIntegerPref(PrefsCollectionApp::ExtAppParamSpaceSubst);
			if (0 == extApp.Length())
			{
				OpStatus::Ignore(SimpleDialog::ShowDialog(WINDOW_NAME_MESSAGE_DIALOG, NULL, Str::SI_IDSTR_MSG_NO_EXT_MAIL_APP_DEFINED_TXT, Str::SI_IDSTR_MSG_NO_EXT_MAIL_APP_DEFINED_CAP, Dialog::TYPE_OK, Dialog::IMAGE_ERROR));
				return;
			}

			OpString command;
			if (OpStatus::IsError(command.Set(extApp.CStr())))
			{
				// TODO - Diplay some more information
				OpStatus::Ignore(SimpleDialog::ShowDialog(WINDOW_NAME_MESSAGE_DIALOG, NULL, Str::S_MSG_WARNING, Str::S_OPERA, Dialog::TYPE_OK, Dialog::IMAGE_WARNING));
				return;	
			}

			uni_char szNoParam[16] = UNI_L("-");
			if (!fExtAppSpaceSubst)
				*szNoParam = 0;
			
			RETURN_VOID_IF_ERROR(command.ReplaceAll(UNI_L("%t"), to && *to ? to : szNoParam));
			RETURN_VOID_IF_ERROR(command.ReplaceAll(UNI_L("%c"), cc && *cc ? cc : szNoParam));
			RETURN_VOID_IF_ERROR(command.ReplaceAll(UNI_L("%b"), bcc && *bcc ? bcc : szNoParam));
			RETURN_VOID_IF_ERROR(command.ReplaceAll(UNI_L("%s"), subject && *subject ? subject : szNoParam));
			RETURN_VOID_IF_ERROR(command.ReplaceAll(UNI_L("%m"), message && *message ? message : szNoParam));

			StrTrimLeft( command, UNI_L(" \t"));

			DWORD dwErr;

			// TODO: Remove the use of OpExecute and get rid of exec.cpp
			// Use g_op_system_info->ExecuteApplication() instead
			if (!OpExecute(command.CStr(), g_main_hwnd, FALSE, FALSE, &dwErr))
			{
				OpString errMsg;
				g_languageManager->GetString(Str::SI_IDSTR_MSG_FAILED_STARTING_EXT_MAIL_APP_TXT, errMsg);
				errMsg.Append("\n");
				errMsg.Append(command);
				errMsg.Append("\n\n");

				const int MAX_SIZE = 256;
				uni_char* sys_error = OP_NEWA(uni_char, MAX_SIZE);
				if (!sys_error)
					return;

				GetSystemErrorText(dwErr, sys_error, MAX_SIZE - 1);

				errMsg.Append(sys_error);
				OP_DELETEA(sys_error);

				OpString title, msg;
				if (OpStatus::IsSuccess(g_languageManager->GetString(Str::SI_IDSTR_MSG_FAILED_STARTING_EXT_MAIL_APP_CAP, title)))
					OpStatus::Ignore(SimpleDialog::ShowDialog(WINDOW_NAME_MESSAGE_DIALOG, NULL, errMsg.CStr(), title.CStr(), Dialog::TYPE_OK, Dialog::IMAGE_ERROR));
			}
			break;	
		} 


		case MAILHANDLER_SYSTEM:
		{
			if (raw_address)
				ShellExecute(g_main_hwnd, NULL, raw_address, NULL, UNI_L(""), SW_SHOWNORMAL);

			break;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////
 
OpString WindowsOpSystemInfo::GetLanguageFolder(const OpStringC &lang_code)
{
	OpString ret; 

	ret.Set(lang_code.CStr()); 

	return ret;
}

////////////////////////////////////////////////////////////////////////////////////

#ifdef AUTO_UPDATE_SUPPORT
OP_STATUS WindowsOpSystemInfo::GetOSName(OpString &os_string)
{
	return os_string.Set("Windows");
}

OP_STATUS WindowsOpSystemInfo::GetOSVersion(OpString &os_version)
{
	OSVERSIONINFO osvi;
	ZeroMemory(&osvi, sizeof(osvi));
	osvi.dwOSVersionInfoSize = sizeof(osvi);

	//	Get OS version
	if ( !GetVersionEx(&osvi) )
	{
		return OpStatus::ERR;
	}
	else
	{
		os_version.Empty();
		os_version.AppendFormat(UNI_L("%d.%d"), osvi.dwMajorVersion, osvi.dwMinorVersion);
		return OpStatus::OK;
	}
}

#endif // AUTO_UPDATE_SUPPORT

#ifdef QUICK_USE_DEFAULT_BROWSER_DIALOG
BOOL WindowsOpSystemInfo::ShallWeTryToSetOperaAsDefaultBrowser()
{
	const BOOL this_is_the_first_run = (g_application->DetermineFirstRunType() == Application::RUNTYPE_FIRSTCLEAN) ? TRUE : FALSE;


	OperaInstaller installer;
	OperaInstaller::Settings settings = installer.GetSettings();
	if (IsSystemWin8())
	{
		if( !this_is_the_first_run && installer.AssociationsAPIReady() &&
			g_desktop_product->GetProductType() != PRODUCT_TYPE_OPERA_LABS &&
			g_pcui->GetIntegerPref(PrefsCollectionUI::ShowDefaultBrowserDialog) &&
			!settings.copy_only &&
			(!installer.HasAssociation(OperaInstaller::HTTP) ||
			!installer.HasAssociation(OperaInstaller::HTTPS)) &&
			!KioskManager::GetInstance()->GetEnabled())
		{
			return TRUE;
		}
	}
	else
	{
		if( !this_is_the_first_run && installer.AssociationsAPIReady() &&
			g_desktop_product->GetProductType() != PRODUCT_TYPE_OPERA_LABS &&
			g_pcui->GetIntegerPref(PrefsCollectionUI::ShowDefaultBrowserDialog) &&
			!settings.copy_only &&
			(!installer.HasAssociation(OperaInstaller::HTM) ||
			!installer.HasAssociation(OperaInstaller::HTML) ||
			!installer.HasAssociation(OperaInstaller::HTTP) ||
			!installer.HasAssociation(OperaInstaller::HTTPS) ||
			!installer.IsDefaultBrowser()) &&
			!KioskManager::GetInstance()->GetEnabled())
		{
			return TRUE;
		}
	}
	return FALSE;
}

OP_STATUS WindowsOpSystemInfo::SetAsDefaultBrowser()
{
	OperaInstaller installer;
	if (!installer.AssociationsAPIReady())
		return OpStatus::ERR;

	installer.SetDefaultBrowserAndAssociations();
	TRAPD(rc, g_pcui->WriteIntegerL(PrefsCollectionUI::ShowDefaultBrowserDialog, FALSE));
	return OpStatus::OK;
}
#endif //QUICK_USE_DEFAULT_BROWSER_DIALOG

HRESULT WindowsFirewallAppIsEnabled(
            INetFwProfile* fwProfile,
            const uni_char* fwProcessImageFileName,
            BOOL* fwAppEnabled
            )
{
    HRESULT hr = S_OK;
    BSTR fwBstrProcessImageFileName = NULL;
    VARIANT_BOOL fwEnabled;
    INetFwAuthorizedApplication* fwApp = NULL;
    INetFwAuthorizedApplications* fwApps = NULL;

    *fwAppEnabled = FALSE;

	do{
		// Retrieve the authorized application collection.
		hr = fwProfile->get_AuthorizedApplications(&fwApps);
		if (FAILED(hr) || !fwApps)
		{
			break;
		}

		// Allocate a BSTR for the process image file name.
		fwBstrProcessImageFileName = SysAllocString(fwProcessImageFileName);
		if (fwBstrProcessImageFileName == NULL)
		{
			hr = E_OUTOFMEMORY;
			break;
		}

		// Attempt to retrieve the authorized application.
		hr = fwApps->Item(fwBstrProcessImageFileName, &fwApp);
		if (SUCCEEDED(hr))
		{
			// Find out if the authorized application is enabled.
			hr = fwApp->get_Enabled(&fwEnabled);
			if (FAILED(hr))
			{
				break;
			}

			if (fwEnabled != VARIANT_FALSE)
			{
				// The authorized application is enabled.
				*fwAppEnabled = TRUE;
			}
		}
		else
		{
			// The authorized application was not in the collection.
			hr = S_OK;
		}
	}while(FALSE);

    // Free the BSTR.
    SysFreeString(fwBstrProcessImageFileName);

    // Release the authorized application instance.
    if (fwApp != NULL)
    {
        fwApp->Release();
    }

    // Release the authorized application collection.
    if (fwApps != NULL)
    {
        fwApps->Release();
    }

    return hr;
}

HRESULT WindowsFirewallInitialize(INetFwProfile** fwProfile)
{
    HRESULT hr = S_OK;
    INetFwMgr* fwMgr = NULL;
    INetFwPolicy* fwPolicy = NULL;

    *fwProfile = NULL;

	do{
		// Create an instance of the firewall settings manager.
		hr = CoCreateInstance(
				__uuidof(NetFwMgr),
				NULL,
				CLSCTX_INPROC_SERVER,
				__uuidof(INetFwMgr),
				(void**)&fwMgr
				);
		if (FAILED(hr))
			break;

		// Retrieve the local firewall policy.
		hr = fwMgr->get_LocalPolicy(&fwPolicy);
		if (FAILED(hr))
			break;

		// Retrieve the firewall profile currently in effect.
		hr = fwPolicy->get_CurrentProfile(fwProfile);
	}while(FALSE);

    // Release the local firewall policy.
    if (fwPolicy != NULL)
    {
        fwPolicy->Release();
    }

    // Release the firewall settings manager.
    if (fwMgr != NULL)
    {
        fwMgr->Release();
    }

    return hr;
}


HRESULT WindowsFirewallAddApp(
            INetFwProfile* fwProfile,
            const uni_char* fwProcessImageFileName,
            const uni_char* fwName
            )
{
    HRESULT hr = S_OK;
    BOOL fwAppEnabled;
    BSTR fwBstrName = NULL;
    BSTR fwBstrProcessImageFileName = NULL;
    INetFwAuthorizedApplication* fwApp = NULL;
    INetFwAuthorizedApplications* fwApps = NULL;

	do{
		// First check to see if the application is already authorized.
		hr = WindowsFirewallAppIsEnabled(
				fwProfile,
				fwProcessImageFileName,
				&fwAppEnabled
				);

		if (FAILED(hr))
			break;

		// Only add the application if it isn't already authorized.
		if (!fwAppEnabled)
		{
			// Retrieve the authorized application collection.
			hr = fwProfile->get_AuthorizedApplications(&fwApps);
			if (FAILED(hr) || !fwApps)
				break;

			// Create an instance of an authorized application.
			hr = CoCreateInstance(
					__uuidof(NetFwAuthorizedApplication),
					NULL,
					CLSCTX_INPROC_SERVER,
					__uuidof(INetFwAuthorizedApplication),
					(void**)&fwApp
					);
			if (FAILED(hr))
				break;

			// Allocate a BSTR for the process image file name.
			fwBstrProcessImageFileName = SysAllocString(fwProcessImageFileName);
			if (fwBstrProcessImageFileName == NULL)
			{
				hr = E_OUTOFMEMORY;
				break;
			}

			// Set the process image file name.
			hr = fwApp->put_ProcessImageFileName(fwBstrProcessImageFileName);
			if (FAILED(hr))
				break;

			// Allocate a BSTR for the application friendly name.
			fwBstrName = SysAllocString(fwName);
			if (SysStringLen(fwBstrName) == 0)
			{
				hr = E_OUTOFMEMORY;
				break;
			}

			// Set the application friendly name.
			hr = fwApp->put_Name(fwBstrName);
			if (FAILED(hr))
				break;

			// Add the application to the collection.
			hr = fwApps->Add(fwApp);
		}
	}while(FALSE);

    // Free the BSTRs.
    SysFreeString(fwBstrName);
    SysFreeString(fwBstrProcessImageFileName);

    // Release the authorized application instance.
    if (fwApp != NULL)
    {
        fwApp->Release();
    }

    // Release the authorized application collection.
    if (fwApps != NULL)
    {
        fwApps->Release();
    }

    return hr;
}


OP_STATUS WindowsOpSystemInfo::AddToWindowsFirewallException(const OpStringC &exe_path, const OpStringC &entry_name)
{
	HRESULT hr = S_OK;
    INetFwProfile* fwProfile = NULL;

	do{
		hr = WindowsFirewallInitialize(&fwProfile);
		if(FAILED(hr) || !fwProfile)
			break;

		hr = WindowsFirewallAddApp(fwProfile,exe_path.CStr(), entry_name.CStr());
		if(FAILED(hr))
			break;

	}while(FALSE);

	if(fwProfile)
		fwProfile->Release();

	return hr == S_OK ? OpStatus::OK : OpStatus::ERR;
}

OP_STATUS WindowsOpSystemInfo::GetDefaultWindowTitle(OpString& title)
{
	switch(g_desktop_product->GetProductType())
	{
		case PRODUCT_TYPE_OPERA_NEXT:
			return title.Set(UNI_L("Opera Next"));
		case PRODUCT_TYPE_OPERA_LABS:
			return title.Set(UNI_L("Opera Labs"));
		default:
			return title.Set(UNI_L("Opera"));
	}
}

void WindowsOpSystemInfo::ParseJavaVersionString(const uni_char* version, int& major, int& minor) const
{
	if (version)
	{
		uni_char verstr[16];
		uni_char *majstr, *minstr=NULL, *tmpstr;
		majstr = tmpstr = verstr;

		uni_strlcpy(verstr, version, 15);
		verstr[15] = '\0';

		while (*tmpstr!='\0')
		{
			if (*tmpstr=='.')
			{
				if (minstr == NULL)
					minstr = tmpstr+1;
				*tmpstr='\0';
			}
			tmpstr++;
		}

		major = uni_atoi(majstr);
		minor = minstr ? uni_atoi(minstr) : 0;
	}
}

OP_STATUS WindowsOpSystemInfo::GetExternalDownloadManager(OpVector<ExternalDownloadManager>& download_managers)
{
	ExternalDownloadManager* thunder = ThunderDownloadManager::Construct();
	if (thunder)
		download_managers.Add(thunder);
	
	ExternalDownloadManager* qq = QQDownloadManager::Construct();
	if (qq)
		download_managers.Add(qq);

	ExternalDownloadManager* flashget = FlashgetDownloadManager::Construct();
	if (flashget)
		download_managers.Add(flashget);	
	
	return OpStatus::OK;
}

OP_STATUS WindowsOpSystemInfo::GetBinaryPath(OpString *path)
{
	if (!path)
		return OpStatus::ERR_NULL_POINTER;

	return GetExePath(*path);
}

OP_STATUS WindowsOpSystemInfo::GetProcessTime(double *time)
{
	*time = 0;
	FILETIME creation_time, exit_time, kernel_time, user_time;

	BOOL ret = GetProcessTimes(GetCurrentProcess(), &creation_time, &exit_time, &kernel_time, &user_time);
	if (!ret)
		return OpStatus::ERR_NO_ACCESS;

	ULARGE_INTEGER kernel, user;
	kernel.LowPart = kernel_time.dwLowDateTime;
	kernel.HighPart = kernel_time.dwHighDateTime;

	user.LowPart = user_time.dwLowDateTime;
	user.HighPart = user_time.dwHighDateTime;

	ULONGLONG total = kernel.QuadPart + user.QuadPart;
	*time = static_cast<double>(total /= 10000);
	return OpStatus::OK;
}

OP_STATUS WindowsOpSystemInfo::GetImage(DesktopOpSystemInfo::PLATFORM_IMAGE platform_image_id, Image &image)
{
	OP_STATUS status = OpStatus::ERR;
	SHSTOCKICONINFO sii;
	::ZeroMemory(&sii,sizeof(SHSTOCKICONINFO));
	sii.cbSize = sizeof(sii);
	
	SHSTOCKICONID image_id = SIID_MAX_ICONS;
	if (platform_image_id == PLATFORM_IMAGE_SHIELD)
	{
		image_id = SIID_SHIELD;
	}

	if ( image_id < SIID_MAX_ICONS && SUCCEEDED(OPSHGetStockIconInfo(image_id, SHGSI_ICON | SHGSI_SMALLICON, &sii)))
	{
		OpBitmap* bitmap=NULL;
		if (OpStatus::IsSuccess(IconUtils::CreateIconBitmap(&bitmap,sii.hIcon)))
		{
			OP_ASSERT(imgManager);
			image = imgManager->GetImage(bitmap);
		}
		DestroyIcon(sii.hIcon);

		status = OpStatus::OK;
	}

	return status;
}

OP_STATUS WindowsOpSystemInfo::CancelNestedMessageLoop()
{
	::EndMenu();
	return OpStatus::OK;
}

#ifdef OPSYSTEMINFO_CPU_FEATURES
unsigned int WindowsOpSystemInfo::GetCPUFeatures()
{
	unsigned int ret = 0;

#ifndef _WIN64
	if (__sse2_available)
#endif
	{
		ret  |= CPU_FEATURES_IA32_SSE2;

		int registers[4];
		__cpuid(registers, 1);

		if (registers[2] & 1)
			ret  |= CPU_FEATURES_IA32_SSE3;

		if (registers[2] & (1 <<  9))
			ret |= CPU_FEATURES_IA32_SSSE3;
	}

	return ret;
}
#endif //OPSYSTEMINFO_CPU_FEATURES

#if(WINVER < 0x0601)
#define SM_DIGITIZER            94

/*
 * GetSystemMetrics(SM_DIGITIZER) flag values
 */
#define NID_INTEGRATED_TOUCH  0x00000001
#define NID_EXTERNAL_TOUCH    0x00000002
#define NID_INTEGRATED_PEN    0x00000004
#define NID_EXTERNAL_PEN      0x00000008
#define NID_MULTI_INPUT       0x00000040
#define NID_READY             0x00000080

#endif // (WINVER < 0x0601)

bool WindowsOpSystemInfo::IsTouchInputAvailable()
{
	if(IsSystemWin7() && g_pcmswin->GetIntegerPref(PrefsCollectionMSWIN::EnableTouchUI) != 0)
	{
		int flags = GetSystemMetrics(SM_DIGITIZER);

		// check for multi touch device
		if(flags & NID_MULTI_INPUT)
			return true;
	}
	return false;
}

bool WindowsOpSystemInfo::IsTouchUIWanted()
{
	if(g_pcmswin->GetIntegerPref(PrefsCollectionMSWIN::EnableTouchUI) == 2)	// force on
		return true;

	return IsTouchInputAvailable();
}

OP_STATUS WindowsOpSystemInfo::GetHardwareFingerprint(OpString8& fingerprint)
{
	if (!m_hardware_fingerprint_generated)
	{
		// MachineGuid should be good enough as fingerprint for search protection.
		// Login name is added just in case MachineGuid doesn't exist in "Cryptography".
		OpString fingerprint;
		RETURN_IF_MEMORY_ERROR(OpSafeRegQueryValue(HKEY_LOCAL_MACHINE, UNI_L("SOFTWARE\\Microsoft\\Cryptography"), fingerprint, UNI_L("MachineGuid"), KEY_WOW64_64KEY));
		uni_char namebuf[UNLEN+1];
		DWORD bufsize = UNLEN+1;
		if (GetUserName(namebuf, &bufsize))
		{
			RETURN_IF_ERROR(fingerprint.Append(namebuf));
		}
		RETURN_IF_ERROR(m_hardware_fingerprint.SetUTF8FromUTF16(fingerprint));
		m_hardware_fingerprint_generated = true;
	}
	return fingerprint.Set(m_hardware_fingerprint);
}
