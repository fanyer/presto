/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "platforms/quix/toolkits/kde4/Kde4UiSettings.h"

#include <KDE/KApplication>
#include <QPalette>
#include <stdio.h>

#ifdef DEBUG
#include <cstdio>
#endif

uint32_t Kde4UiSettings::GetSystemColor(SystemColor color)
{
	QColor col;
	QPalette pal = m_application->palette();

	switch (color)
	{
		case COLOR_BUTTON:
			col = pal.color(QPalette::Active, QPalette::Button); break;
		case COLOR_BUTTON_LIGHT:
			col = pal.color(QPalette::Active, QPalette::Midlight); break;
		case COLOR_BUTTON_DARK:
			col = pal.color(QPalette::Active, QPalette::Dark); break;
		case COLOR_BUTTON_VERYDARK:
			col = pal.color(QPalette::Active, QPalette::Shadow); break;
		case COLOR_BUTTON_TEXT:
			col = pal.color(QPalette::Active, QPalette::ButtonText); break;
		case COLOR_BACKGROUND:
			col = pal.color(QPalette::Active, QPalette::Base); break;
		case COLOR_BACKGROUND_SELECTED:
			col = pal.color(QPalette::Active, QPalette::Highlight); break;
		case COLOR_BACKGROUND_SELECTED_NOFOCUS:
			col = pal.color(QPalette::Active, QPalette::Background); break;
		case COLOR_BACKGROUND_DISABLED:
			col = pal.color(QPalette::Active, QPalette::Background); break;
		case COLOR_UI_MENU: /* fallthrough */
		case COLOR_TEXT:
			col = pal.color(QPalette::Active, QPalette::Text); break;
		case COLOR_TEXT_SELECTED:
			col = pal.color(QPalette::Active, QPalette::HighlightedText); break;
		case COLOR_TEXT_SELECTED_NOFOCUS:
			col = pal.color(QPalette::Active, QPalette::ButtonText); break;
		case COLOR_TEXT_DISABLED:
			col = pal.color(QPalette::Disabled, QPalette::Text); break;
		case COLOR_TEXT_HIGHLIGHTED:
			col = QColor(255, 255, 255); break;
		case COLOR_TEXT_HIGHLIGHTED_NOFOCUS:
			col = QColor(0, 0, 0); break;
		case COLOR_BACKGROUND_HIGHLIGHTED:
			col = QColor(35, 220, 35); break; //Green
		case COLOR_BACKGROUND_HIGHLIGHTED_NOFOCUS:
			col = QColor(255, 255, 0); break; //Yellow
		case COLOR_SCROLLBAR_BACKGROUND:
			col = pal.color(QPalette::Active, QPalette::Mid); break;
		case COLOR_UI_BUTTON_HOVER:
			col = pal.color(QPalette::Active, QPalette::Highlight); break;
		case COLOR_UI_FONT:
			col = pal.color(QPalette::Active, QPalette::Text); break;
		case COLOR_UI_BACKGROUND:
			col = pal.color(QPalette::Active, QPalette::Background); break;
		case COLOR_UI_DISABLED_FONT:
			col = pal.color(QPalette::Disabled, QPalette::Text); break;
		case COLOR_SKIN:
			col = pal.color(QPalette::Active,QPalette::Button); break;
		case COLOR_LINK:
			col = QColor(0, 0, 204); break;
		case COLOR_VISITED_LINK:
			col = QColor(128, 0, 128); break;
#if QT_VERSION >= 0x040400
		case COLOR_TOOLTIP_BACKGROUND:
			col = pal.color(QPalette::Active, QPalette::ToolTipBase); break;
		case COLOR_TOOLTIP_TEXT:
			col = pal.color(QPalette::Active, QPalette::ToolTipText); break;
#endif
		case COLOR_WORKSPACE:
			col = pal.color(QPalette::Active, QPalette::Dark); break;
		default:
#ifdef DEBUG
			fprintf(stderr, "Kde4UiSettings::GetSystemColor(): Unknown color code %d. Using red.\n", color);
			col = Qt::red;
#else
			col = Qt::white;
#endif
			break;
	}

	return MakeColor(col);
}

bool Kde4UiSettings::GetDefaultFont(FontDetails& font)
{
	return true;
}
