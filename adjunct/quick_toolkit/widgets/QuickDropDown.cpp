/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/QuickDropDown.h"

/*virtual*/
QuickDropDown::~QuickDropDown()
{
	GetOpWidget()->SetItemAddedHandler(NULL);
}

/*virtual*/ OP_STATUS
QuickDropDown::Init()
{
	RETURN_IF_ERROR(QuickEditableTextWidgetWrapper<DesktopOpDropDown>::Init());
	GetOpWidget()->SetItemAddedHandler(
			MAKE_DELEGATE(*this, &QuickDropDown::OnItemAdded));
	return OpStatus::OK;
}
