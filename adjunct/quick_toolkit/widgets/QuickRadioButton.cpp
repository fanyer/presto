/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Cihat Imamoglu (cihati)
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/QuickRadioButton.h"

OP_STATUS QuickRadioButton::ConstructSelectableQuickWidget()
{
	GetOpWidget()->SetButtonType(OpButton::TYPE_RADIO);
	GetOpWidget()->SetJustify(JUSTIFY_LEFT, FALSE);
	RETURN_IF_ERROR(FindImageSizes("Radio Button Skin"));
	return OpStatus::OK;
}
