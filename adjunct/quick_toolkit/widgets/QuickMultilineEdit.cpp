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

#include "adjunct/quick_toolkit/widgets/QuickMultilineEdit.h"

#include "adjunct/desktop_util/widget_utils/VisualDeviceHandler.h"

OP_STATUS QuickMultilineEdit::SetText(const OpStringC& text)
{
	// We need to override this method because text in a multiline edit
	// widget cannot be set without a visual device
	VisualDeviceHandler handler(GetOpWidget());

	return Base::SetText(text);
}
