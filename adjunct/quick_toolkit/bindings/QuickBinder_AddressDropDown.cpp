/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/bindings/QuickBinder_AddressDropDown.h"


QuickBinder::QuickAddressDropDownTextBinding::~QuickAddressDropDownTextBinding()
{
	if (m_widget)
		static_cast<QuickAddressDropDown*>(m_widget)->GetOpWidget()->SetEditListener(NULL);
}

OP_STATUS QuickBinder::QuickAddressDropDownTextBinding::Init(
		QuickAddressDropDown& drop_down, OpProperty<OpString>& property)
{
	RETURN_IF_ERROR(EditableTextPropertyBinding::Init(drop_down, property));
	drop_down.GetOpWidget()->SetEditListener(this);
	return OpStatus::OK;
}

void QuickBinder::QuickAddressDropDownTextBinding::OnTreeViewDropdownEditChange(OpTreeViewDropdown* dropdown, const OpStringC& text)
{
	// The listener can be set to NULL in QuickWindow::OnDesktopWindowClosing(...), so check to make sure
	if (dropdown->GetListener())
		dropdown->GetListener()->OnChange(dropdown);
}
