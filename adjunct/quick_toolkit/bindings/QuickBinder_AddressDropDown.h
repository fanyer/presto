/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef QUICK_BINDER_ADDRESS_DROP_DOWN_H
#define QUICK_BINDER_ADDRESS_DROP_DOWN_H

#include "adjunct/quick_toolkit/bindings/QuickBinder_Text.h"
#include "adjunct/quick_toolkit/widgets/QuickAddressDropDown.h"


/**
 * A bidirectional binding for the text of a QuickAddressDropDown.
 *
 * The widget text is bound to a string OpProperty.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class QuickBinder::QuickAddressDropDownTextBinding
		: public QuickBinder::EditableTextPropertyBinding
		, public OpTreeViewDropdown::EditListener
{
public:
	virtual ~QuickAddressDropDownTextBinding();

	OP_STATUS Init(QuickAddressDropDown& drop_down, OpProperty<OpString>& property);

	// OpTreeViewDropdown::EditListener
	virtual void OnTreeViewDropdownEditChange(OpTreeViewDropdown* dropdown, const OpStringC& text);
};

#endif // QUICK_BINDER_ADDRESS_DROP_DOWN_H
