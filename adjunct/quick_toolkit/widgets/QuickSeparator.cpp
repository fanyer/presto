/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/QuickSeparator.h"

QuickSeparator* QuickSeparator::ConstructSeparator(const OpStringC8& skin_element)
{
	OpAutoPtr<QuickSeparator> separator(OP_NEW(QuickSeparator, ()));
	RETURN_VALUE_IF_NULL(separator.get(), NULL);
	RETURN_VALUE_IF_ERROR(separator->Init(), NULL);
	if (skin_element.HasContent())
		separator->SetSkin(skin_element);

	return separator.release();
}

unsigned QuickSeparator::GetDefaultMinimumWidth()
{	
	return 0;
}

unsigned QuickSeparator::GetDefaultMinimumHeight(unsigned width)
{
	INT32 w,h;
	GetOpWidget()->GetBorderSkin()->GetSize(&w,&h);
	return (unsigned)h;
}

unsigned QuickSeparator::GetDefaultPreferredWidth()
{
	return WidgetSizes::Fill;
}

unsigned QuickSeparator::GetDefaultPreferredHeight(unsigned width)
{
	return GetDefaultMinimumHeight(width);
}
