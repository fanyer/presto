/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/QuickSkinElement.h"

QuickSkinElement* QuickSkinWrap(QuickWidget* widget,
		const OpStringC8& skin_element, const OpStringC8& fallback)
{
	if (!widget)
		return NULL;

	OpAutoPtr<QuickWidget> widget_holder(widget);
	OpAutoPtr<QuickSkinElement> skinned_widget(OP_NEW(QuickSkinElement, ()));
	RETURN_VALUE_IF_NULL(skinned_widget.get(), NULL);
	RETURN_VALUE_IF_ERROR(skinned_widget->Init(), NULL);
	skinned_widget->SetSkin(skin_element, fallback);
	skinned_widget->SetContent(widget_holder.release());
	return skinned_widget.release();
}
