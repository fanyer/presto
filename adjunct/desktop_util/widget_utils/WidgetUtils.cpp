/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patricia Aas (psmaas)
 */

#include "core/pch.h"

#include "adjunct/desktop_util/widget_utils/WidgetUtils.h"
#include "modules/widgets/WidgetContainer.h"

OpFont* GetOpFont(const WIDGET_FONT_INFO& font_info)
{
    // Make a font attribute to query the font cache
    FontAtt fontatt;
    fontatt.SetFaceName(font_info.font_info->GetFace());
    fontatt.SetSize(font_info.size);
    fontatt.SetWeight(font_info.weight);
    fontatt.SetItalic(font_info.italic);
    
    // Query the font cache
    return g_font_cache->GetFont(fontatt, 100);
}

int WidgetUtils::GetFontHeight(const WIDGET_FONT_INFO& font_info)
{
	OpFont* font = GetOpFont(font_info);
    int font_height = max(font->Height(), font->Ascent() + font->Descent());
    g_font_cache->ReleaseFont(font);
    
    return font_height;
}

unsigned WidgetUtils::GetFontWidth(const WIDGET_FONT_INFO& font_info)
{
	OpFont* font = GetOpFont(font_info);
	unsigned font_width = font->AveCharWidth();
	g_font_cache->ReleaseFont(font);

	return font_width;
}

unsigned WidgetUtils::GetStringWidth(const WIDGET_FONT_INFO& font_info, const uni_char* str)
{
	OpFont* font = GetOpFont(font_info);
	UINT32 len = font->StringWidth(str, uni_strlen(str), font_info.char_spacing_extra);
	g_font_cache->ReleaseFont(font);

	return len;
}

void WidgetUtils::SetFocusInDialogPage(OpWidget* page)
{
	WidgetContainer* container = page->GetWidgetContainer();
	if (container)
	{
		OpWidget* to_focus = container->GetNextFocusableWidget(page, TRUE);
		if (to_focus)
		{
			to_focus->SetFocus(FOCUS_REASON_OTHER);
		}
	}
}
