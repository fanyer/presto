/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/QuickMultilineLabel.h"
#include "adjunct/desktop_util/widget_utils/VisualDeviceHandler.h"

OP_STATUS QuickMultilineLabel::Init()
{
	RETURN_IF_ERROR(QuickMultilineEdit::Init());

	GetOpWidget()->SetLabelMode();
	SetSkin("MultilineLabel Edit Skin");

	return OpStatus::OK;
}

unsigned QuickMultilineLabel::GetDefaultMinimumWidth()
{
	INT32 left, top, right, bottom;
	GetOpWidget()->GetPadding(&left, &top, &right, &bottom);
	return min(unsigned(GetOpWidget()->GetMaxBlockWidth() + left + right),
			WidgetUtils::GetFontWidth(GetOpWidget()->font_info) * 40);
}

unsigned QuickMultilineLabel::GetDefaultPreferredWidth()
{
	INT32 left, top, right, bottom;
	GetOpWidget()->GetPadding(&left, &top, &right, &bottom);
	OpString text;
	VisualDeviceHandler handler(GetOpWidget());
	RETURN_VALUE_IF_ERROR(GetText(text), GetDefaultMinimumWidth());;
	return WidgetUtils::GetStringWidth(GetOpWidget()->font_info, text) + left + right;
}

unsigned QuickMultilineLabel::GetDefaultMinimumHeight(unsigned width)
{
	if (width == WidgetSizes::UseDefault)
		width = GetMinimumWidth();

	const OpRect rect = GetOpWidget()->GetRect();	
	bool changed_rect = false;
	VisualDeviceHandler handler(GetOpWidget());
	
	if (rect.width != int(width))
	{		
		GetOpWidget()->SetRect(OpRect(0, 0, min(width, 100000U), 100000));
		changed_rect = true;
	}

	unsigned height = 0;
	if (!GetOpWidget()->GetRect().IsEmpty())
	{
		INT32 left, top, right, bottom;
		GetOpWidget()->GetPadding(&left, &top, &right, &bottom);
		height = unsigned(GetOpWidget()->GetContentHeight() + top + bottom);
	}
   
	if (changed_rect)
		GetOpWidget()->SetRect(rect);
		
	return height;
}
