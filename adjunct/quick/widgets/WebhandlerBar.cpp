/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "WebhandlerBar.h"

#include "adjunct/quick/widgets/OpInfobar.h"


// static
OP_STATUS WebhandlerBar::Construct(WebhandlerBar** obj)
{
	*obj = OP_NEW(WebhandlerBar, ());
	if (*obj == NULL || OpStatus::IsError((*obj)->init_status))
	{
		OP_DELETE(*obj);
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}



WebhandlerBar::WebhandlerBar(PrefsCollectionUI::integerpref prefs_setting, PrefsCollectionUI::integerpref autoalignment_prefs_setting)
	: OpInfobar(prefs_setting, autoalignment_prefs_setting)
{
}


void WebhandlerBar::OnReadContent(PrefsSection* section)
{
	OpInfobar::OnReadContent(section);
	if (GetParent() && (m_status_text.HasContent() || m_question_text.HasContent()))
	{
		// DSK-347641 We have to update the layout and set widget sizes (by using FALSE in first
		// argument) due to that we use multi line widgets. They need the width to report a proper height
		INT32 used_width, used_height;
		OnLayout(FALSE, GetParent()->GetWidth(), GetParent()->GetHeight(), used_width, used_height);
	}
}
