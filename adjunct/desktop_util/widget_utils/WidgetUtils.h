/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#ifndef WIDGET_UTILS_H
#define WIDGET_UTILS_H

#include "modules/widgets/OpWidget.h"

namespace WidgetUtils
{
	/**
	 * Gets the height of text drawn with the font indicated by the font_info 
	 * @param font_info indicating the font in question
	 * @return the height of the font
	 */
    int GetFontHeight(const WIDGET_FONT_INFO& font_info);

	/**
	 * Gets the average width of characters drawn with the font indicated by the font_info
	 * @param font_info the font to check
	 * @return Average character width
	 */
	unsigned GetFontWidth(const WIDGET_FONT_INFO& font_info);

	/**
	 * Gets the exact width of the string with the font indicated by the font_info
	 * @param font_info the font to check
	 * @param str the string to measure
	 * @return Average character width
	 */
	unsigned GetStringWidth(const WIDGET_FONT_INFO& font_info, const uni_char* str);

	/**
	 * Makes a widget in a dialog page receive the keyboard focus.
	 * @param page any widget containing other widgets
	 */
	void SetFocusInDialogPage(OpWidget* page);
};

#endif // WIDGET_UTILS_H
