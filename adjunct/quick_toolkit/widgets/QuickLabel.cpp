/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Cihat Imamoglu (cihati)
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/QuickLabel.h"
#include "modules/widgets/OpWidget.h"
#include "adjunct/desktop_util/widget_utils/VisualDeviceHandler.h"

OP_STATUS QuickLabel::Init()
{
	RETURN_IF_ERROR(Base::Init());

	SetSkin("DesktopLabel Skin");
	return OpStatus::OK;
}

unsigned QuickLabel::GetDefaultMinimumWidth()
{
	VisualDeviceHandler handler(GetOpWidget());

	INT32 left, top, right, bottom;
	GetOpWidget()->GetInfo()->GetBorders(GetOpWidget(), left, top, right, bottom);
	unsigned width = GetOpWidget()->string.GetWidth();
	return left + width + right + GetOpWidget()->GetPaddingLeft() + GetOpWidget()->GetPaddingRight();
}

unsigned QuickLabel::GetDefaultMinimumHeight(unsigned width)
{
	INT32 left, top, right, bottom;
	GetOpWidget()->GetInfo()->GetBorders(GetOpWidget(), left, top, right, bottom);
	return GetOpWidget()->string.GetHeight() + GetOpWidget()->GetPaddingBottom() + GetOpWidget()->GetPaddingTop() + top + bottom;
}

QuickLabel* QuickLabel::ConstructLabel(const OpStringC& text)
{
	OpAutoPtr<QuickLabel> label(OP_NEW(QuickLabel, ()));
	RETURN_VALUE_IF_NULL(label.get(), NULL);
	RETURN_VALUE_IF_ERROR(label->Init(), NULL);
	RETURN_VALUE_IF_ERROR(label->SetText(text), NULL);
	return label.release();
}
