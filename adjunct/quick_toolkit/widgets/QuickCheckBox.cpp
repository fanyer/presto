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
#include "adjunct/quick_toolkit/widgets/QuickCheckBox.h"
#include "modules/widgets/OpButton.h"

OP_STATUS QuickCheckBox::ConstructSelectableQuickWidget()
{
	GetOpWidget()->SetButtonType(OpButton::TYPE_CHECKBOX);
	GetOpWidget()->SetJustify(JUSTIFY_LEFT, FALSE);
	RETURN_IF_ERROR(FindImageSizes("Checkbox Skin"));
	return OpStatus::OK;
}
