/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Alexander Remen (alexr@opera.com)
 */

#include "core/pch.h"

#include "adjunct/desktop_util/widget_utils/VisualDeviceHandler.h"
#include "adjunct/quick_toolkit/widgets/OpUnreadBadge.h"

DEFINE_CONSTRUCT(OpUnreadBadge)

OpUnreadBadge::OpUnreadBadge() 
: OpButton(OpButton::TYPE_CUSTOM, OpButton::STYLE_TEXT)
, m_minimum_width(0)
{
#ifdef QUICK_TOOLKIT_ACCORDION_MAC_STYLE // Mac has ugly unread counts that need to be center aligned
	SetJustify(JUSTIFY_CENTER, FALSE);
	init_status = SetText(UNI_L("00"));
#else
	SetJustify(JUSTIFY_RIGHT, FALSE);
	init_status = SetText(UNI_L("0000"));
#endif // QUICK_TOOLKIT_ACCORDION_MAC_STYLE
	CHECK_STATUS(init_status);

	SetDead(TRUE);
	INT32 h;
	VisualDeviceHandler vis_dev_handler(this);
	GetRequiredSize(m_minimum_width, h);
	init_status = SetText(UNI_L(""));
}


void OpUnreadBadge::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	OpButton::GetPreferedSize(w, h, cols, rows);
	*w = max(*w, m_minimum_width);
}

void OpUnreadBadge::OnMouseMove(const OpPoint &point)
{
	parent->OnMouseMove(OpPoint(point.x + rect.x, point.y + rect.y));
}
void OpUnreadBadge::OnMouseLeave()
{
	parent->OnMouseLeave();
}
void OpUnreadBadge::OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	parent->OnMouseDown(OpPoint(rect.x, point.y + rect.y), button, nclicks);
}
void OpUnreadBadge::OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	parent->OnMouseUp(OpPoint(rect.x, point.y + rect.y), button, nclicks);
}
