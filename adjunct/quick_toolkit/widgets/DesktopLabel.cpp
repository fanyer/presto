/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author bkazmierczak
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/DesktopLabel.h"

DEFINE_CONSTRUCT(DesktopLabel);

DesktopLabel::DesktopLabel()
{
	SetFlatMode();
	SetSelectable(false);
	SetEllipsis(ELLIPSIS_END);
}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

OpAccessibleItem* DesktopLabel::GetAccessibleChildOrSelfAt(int x, int y)
{
	OpAccessibleItem* widget = OpWidget::GetAccessibleChildOrSelfAt(x, y);
	if (widget == NULL)
		return NULL;
	else
		return this;
}

OpAccessibleItem* DesktopLabel::GetAccessibleFocusedChildOrSelf()
{
	OpAccessibleItem* widget = OpWidget::GetAccessibleFocusedChildOrSelf();
	if (widget == NULL)
		return NULL;
	else
		return this;
}
#endif
