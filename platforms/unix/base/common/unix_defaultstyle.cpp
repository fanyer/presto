/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Morten Stenshorne, Espen Sand
 */
#include "core/pch.h"

#include "unix_defaultstyle.h"

#include "modules/pi/OpFont.h"
#include "modules/pi/OpScreenInfo.h"

extern INT32 g_forced_dpi; // quick.cpp

namespace UnixDefaultStyle
{
	void GetSystemFont(OP_SYSTEM_FONT type, FontAtt &font)
	{
		enum FontHeightPx {
			HEIGHT_UI=12,
			HEIGHT_FORM=12,
			HEIGHT_NORMAL=16,
			HEIGHT_MAIL_DISPLAY=13 // Changed from 16 (bug #203163)
		};
#define CONVERT_TO_PIXELSIZE(x) ((x*vert_dpi+36)/72)

		static UINT32 vert_dpi = 0;
		if (!vert_dpi)
		{
			UINT32 dummy;
			g_op_screen_info->GetDPI(&dummy, &vert_dpi, NULL, NULL);
			// This is how we limited the DPI values in 8.x. Ideally we
			// should probably make this configurable for the user in some
			// way in 9 [espen 2005-09-20]

			if (vert_dpi > 83)
				vert_dpi = 96;
			else if (vert_dpi >= 63)
				vert_dpi = 72;
			else
				vert_dpi = 54;
		}

		// The forced dpi value gets read from prefs on startup, but we
		// cannot use the prefs function because we are here before the
		// unix-prefs-object has been initialized.

		if( g_forced_dpi > 10 )
		{
			vert_dpi = g_forced_dpi;
		}

		int height = HEIGHT_NORMAL;
		int weight = 4;

		switch (type)
		{
		case OP_SYSTEM_FONT_DOCUMENT_NORMAL:
			break;
		case OP_SYSTEM_FONT_DOCUMENT_HEADER1:
			height = HEIGHT_NORMAL * 2;
			weight = 7;
			break;
		case OP_SYSTEM_FONT_DOCUMENT_HEADER2:
		case OP_SYSTEM_FONT_UI_HEADER:
			height = HEIGHT_NORMAL * 3 / 2;
			weight = 7;
			break;
		case OP_SYSTEM_FONT_DOCUMENT_HEADER3:
			height = HEIGHT_NORMAL * 7 / 6;
			weight = 7;
			break;
		case OP_SYSTEM_FONT_DOCUMENT_HEADER4:
			weight = 7;
			break;
		case OP_SYSTEM_FONT_DOCUMENT_HEADER5:
			height = HEIGHT_NORMAL * 5 / 6;
			weight = 7;
			break;
		case OP_SYSTEM_FONT_DOCUMENT_HEADER6:
			height = HEIGHT_NORMAL * 3 / 4;
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
			height = CONVERT_TO_PIXELSIZE(9);
			if (height < 12)
				height = 12;
			break;
		case OP_SYSTEM_FONT_UI:
		case OP_SYSTEM_FONT_UI_DISABLED:
		case OP_SYSTEM_FONT_UI_HOTLIST_TREE:
		case OP_SYSTEM_FONT_UI_MENU:
			height = HEIGHT_UI;
			break;
		}

		font.SetSize(height);
		font.SetWeight(weight);
		OpFontManager *fontManager = styleManager->GetFontManager();

		switch (type)
		{
		case OP_SYSTEM_FONT_DOCUMENT_PRE:
		case OP_SYSTEM_FONT_FORM_TEXT:
		case OP_SYSTEM_FONT_EMAIL_COMPOSE:
		case OP_SYSTEM_FONT_HTML_COMPOSE:
		case OP_SYSTEM_FONT_EMAIL_DISPLAY:
			font.SetFaceName(fontManager->GetGenericFontName(GENERIC_FONT_MONOSPACE));
			break;
		case OP_SYSTEM_FONT_FORM_BUTTON:
		case OP_SYSTEM_FONT_FORM_TEXT_INPUT:
		case OP_SYSTEM_FONT_UI_MENU:
		case OP_SYSTEM_FONT_UI_TOOLBAR:
		case OP_SYSTEM_FONT_UI_DIALOG:
		case OP_SYSTEM_FONT_UI_PANEL:
		case OP_SYSTEM_FONT_UI_HEADER:
		case OP_SYSTEM_FONT_UI_TOOLTIP:
			font.SetFaceName(fontManager->GetGenericFontName(GENERIC_FONT_SANSSERIF));
			break;
		default:
			font.SetFaceName(fontManager->GetGenericFontName(GENERIC_FONT_SERIF));
		}
	}
}
