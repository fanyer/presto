/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 */

#ifndef QUICK_RICH_TEXT_LABEL_H
#define QUICK_RICH_TEXT_LABEL_H

#include "adjunct/desktop_util/widget_utils/VisualDeviceHandler.h"
#include "adjunct/quick_toolkit/widgets/OpRichTextLabel.h"
#include "adjunct/quick_toolkit/widgets/QuickTextWidget.h"

class QuickRichTextLabel : public QuickTextWidgetWrapper<OpRichTextLabel>
{
	typedef QuickTextWidgetWrapper<OpRichTextLabel> Base;
	IMPLEMENT_TYPEDOBJECT(Base);
public:
	// QuickTextWidget
	virtual OP_STATUS SetText(const OpStringC& text) { VisualDeviceHandler handler(GetOpWidget()); return Base::SetText(text); }
};

#endif //QUICK_RICH_TEXT_LABEL_H
